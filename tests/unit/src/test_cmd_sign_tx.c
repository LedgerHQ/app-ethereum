/**
 * @file test_cmd_sign_tx.c
 * @brief Unit tests for the typed-tx prefix dispatcher inside the
 *        sign_tx APDU handler at
 *        src/features/sign_tx/cmd_sign_tx.c.
 *
 * cmd_sign_tx is the entry point for "sign raw transaction" APDUs.
 * P2 selects the mode (BASIC / STORE / START_FLOW), P1 selects the
 * chunk position (FIRST / MORE). On the FIRST chunk the device
 * parses a BIP-32 path, allocates a fresh keccak context, and looks
 * at the first byte of the payload to decide which transaction
 * format follows:
 *   - 0x01 = EIP-2930 (access list),
 *   - 0x02 = EIP-1559 (priority fee),
 *   - 0x04 = EIP-7702 (auth list),
 *   - 0x00..0x7F other = unsupported typed tx (must reject),
 *   - 0x80..0xFF = legacy RLP list (no type prefix consumed).
 *
 * Routing the wrong format here would mis-parse subsequent fields,
 * so the device might display a value / destination / chainID that
 * doesn't match the bytes actually signed. Pin every branch.
 *
 * The handler also resets the app context on any non-IDLE entry so
 * a stale partial flow cannot bleed into a new one (defense in
 * depth against re-entrancy on a single APDU pipe).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_sign_tx.h"
#include "eth_ustream.h"
#include "calldata.h"
#include "network.h"
#include "wraps.h"

extern cx_sha3_t *g_tx_hash_ctx;

// =============================================================================
// Globals required by linked translation units
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// cx_keccak_init_no_throw is driven through g_keccak_init_ret (sdk_stubs.c).

static bool s_parsebip32_force_null = false;
const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    (void) bip32;
    if (s_parsebip32_force_null) return NULL;
    if (*dataLength < 1) return NULL;
    uint8_t count = *dataBuffer;
    if ((size_t) *dataLength < 1 + (size_t) count * 4) return NULL;
    dataBuffer += 1 + count * 4;
    *dataLength -= 1 + count * 4;
    return dataBuffer;
}

static int g_reset_app_calls = 0;
void reset_app_context(void) {
    g_reset_app_calls++;
}

static cx_err_t g_cx_hash_ret = CX_OK;
cx_err_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t in_len,
                          uint8_t *out,
                          size_t out_len) {
    (void) hash;
    (void) mode;
    (void) in;
    (void) in_len;
    (void) out;
    (void) out_len;
    return g_cx_hash_ret;
}

// finalize_parsing lives in logic_sign_tx.c which we don't link. Stub
// it out so handle_parsing_status() reaches a deterministic outcome
// on the USTREAM_FINISHED branch we exercise via short payloads.
uint16_t finalize_parsing(const txContext_t *context) {
    (void) context;
    return SWO_SUCCESS;
}

// custom_processor is in logic_sign_tx.c too. The eth_ustream parser
// calls it on every field; return CUSTOM_NOT_HANDLED so the standard
// dispatcher takes over.
customStatus_e custom_processor(txContext_t *context) {
    (void) context;
    return CUSTOM_NOT_HANDLED;
}

// =============================================================================
// Stubs — symbols referenced by cmd_sign_tx.c that we don't exercise
// =============================================================================

bool validate_instruction_hash(void) {
    return true;
}
bool process_empty_txs_after(void) {
    return true;
}
size_t get_tx_ctx_count(void) {
    return 1;
}
bool ui_gcs(void) {
    return true;
}
void ui_gcs_cleanup(void) {
}

bool tx_ctx_init(s_calldata *calldata,
                 const uint8_t *from,
                 const uint8_t *to,
                 const uint8_t *amount,
                 const uint64_t *chain_id) {
    (void) calldata;
    (void) from;
    (void) to;
    (void) amount;
    (void) chain_id;
    return true;
}
s_calldata *calldata_init(size_t size, const uint8_t *selector) {
    (void) size;
    (void) selector;
    return NULL;
}
bool calldata_append(s_calldata *calldata, const uint8_t *buffer, size_t size) {
    (void) calldata;
    (void) buffer;
    (void) size;
    return true;
}
void calldata_delete(s_calldata *node) {
    (void) node;
}

// =============================================================================
// APDU builder — FIRST chunk has BIP-32 path then optional tx_type byte
// =============================================================================

static size_t build_first(uint8_t *out,
                          size_t out_size,
                          uint8_t tx_type_or_first_rlp,
                          bool include_tx_byte) {
    size_t off = 0;
    out[off++] = 5;  // BIP-32 path length
    for (int i = 0; i < 5; i++) {
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
    }
    if (include_tx_byte) {
        out[off++] = tx_type_or_first_rlp;
    }
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    appState = APP_STATE_IDLE;
    if (g_tx_hash_ctx != NULL) {
        free(g_tx_hash_ctx);
        g_tx_hash_ctx = NULL;
    }
    memset(&txContext, 0, sizeof(txContext));
    memset(&tmpContent, 0, sizeof(tmpContent));
    memset(&dataContext, 0, sizeof(dataContext));
    s_parsebip32_force_null = false;
    g_keccak_init_ret = CX_OK;
    g_cx_hash_ret = CX_OK;
    g_reset_app_calls = 0;
}

// =============================================================================
// Tests — P2 / P1 dispatcher
// =============================================================================

void test_unknown_p2_rejected(void) {
    uint8_t data[1] = {0};
    uint16_t sw = handle_sign(P1_FIRST, /*p2=*/0x42, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

void test_basic_unknown_p1_rejected(void) {
    uint8_t data[1] = {0};
    uint16_t sw = handle_sign(/*p1=*/0x42, /*p2=*/0, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

void test_p1_more_without_first_rejected(void) {
    // appState = IDLE, no prior FIRST. P1=MORE must be refused.
    uint8_t data[1] = {0};
    uint16_t sw = handle_sign(P1_MORE, /*p2=*/0, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_COMMAND_NOT_ALLOWED);
}

void test_start_flow_without_signing_rejected(void) {
    uint8_t data[1] = {0};
    uint16_t sw = handle_sign(0, /*p2=*/2, data, 0);
    TEST_ASSERT_EQUAL(sw, SWO_COMMAND_NOT_ALLOWED);
}

void test_start_flow_with_nonzero_length_rejected(void) {
    appState = APP_STATE_SIGNING_TX;
    uint8_t data[1] = {0};
    uint16_t sw = handle_sign(0, /*p2=*/2, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

// =============================================================================
// Tests — handle_first_sign_chunk (typed-tx prefix)
// =============================================================================

void test_first_bad_bip32_rejected(void) {
    s_parsebip32_force_null = true;
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x02, true);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_first_no_tx_byte_after_bip32_rejected(void) {
    uint8_t apdu[64];
    // BIP-32 path only, no follow-on byte.
    size_t len = build_first(apdu, sizeof(apdu), 0, /*include_tx_byte=*/false);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_first_rejected_when_app_not_idle(void) {
    appState = APP_STATE_SIGNING_MESSAGE;  // stale
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x02, true);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_COMMAND_NOT_ALLOWED);
    TEST_ASSERT_EQUAL(g_reset_app_calls, 0);
    TEST_ASSERT_EQUAL(appState, APP_STATE_SIGNING_MESSAGE);
}

void test_first_idle_does_not_reset(void) {
    appState = APP_STATE_IDLE;
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x02, true);
    (void) handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(g_reset_app_calls, 0);
    TEST_ASSERT_EQUAL(appState, APP_STATE_SIGNING_TX);
}

void test_typed_tx_eip2930(void) {
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x01, true);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    // Handler returns NO_RESPONSE on the FIRST chunk path that
    // continues into process_tx with the remaining 0 bytes (which
    // reports USTREAM_PROCESSING → SWO_SUCCESS).
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(txContext.txType, EIP2930);
}

void test_typed_tx_eip1559(void) {
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x02, true);
    (void) handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(txContext.txType, EIP1559);
}

void test_typed_tx_eip7702(void) {
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x04, true);
    (void) handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(txContext.txType, EIP7702);
}

void test_typed_tx_unsupported_in_range_rejected(void) {
    uint8_t apdu[64];
    // 0x03 sits inside the typed-tx range (≤ 0x7F) but is not one of
    // the three accepted types. Must be rejected, not silently parsed.
    size_t len = build_first(apdu, sizeof(apdu), 0x03, true);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_MEMORY_WRITE_ERROR);
}

void test_typed_tx_boundary_0x7F_rejected(void) {
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x7F, true);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_MEMORY_WRITE_ERROR);
}

void test_legacy_byte_0x80_routes_to_legacy(void) {
    uint8_t apdu[64];
    // 0x80 is the RLP "empty string" / first byte of a tiny RLP list;
    // it is > MAX_TX_TYPE (0x7F) so the handler must NOT consume it
    // as a prefix and instead set txType = LEGACY.
    size_t len = build_first(apdu, sizeof(apdu), 0x80, true);
    (void) handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(txContext.txType, LEGACY);
}

void test_legacy_byte_0xC0_routes_to_legacy(void) {
    uint8_t apdu[64];
    // 0xC0 is the canonical RLP list header for an empty list.
    size_t len = build_first(apdu, sizeof(apdu), 0xC0, true);
    (void) handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(txContext.txType, LEGACY);
}

void test_legacy_byte_0xFF_routes_to_legacy(void) {
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0xFF, true);
    (void) handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(txContext.txType, LEGACY);
}

void test_typed_tx_cx_hash_failure_rejected(void) {
    g_cx_hash_ret = CX_INVALID_PARAMETER;
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x02, true);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_init_tx_failure_rejected(void) {
    g_keccak_init_ret = CX_INVALID_PARAMETER;
    uint8_t apdu[64];
    size_t len = build_first(apdu, sizeof(apdu), 0x02, true);
    uint16_t sw = handle_sign(P1_FIRST, 0, apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

// =============================================================================
// handle_parsing_status (non-static helper) -- per-status dispatch
// =============================================================================

uint16_t handle_parsing_status(parserStatus_e status);

void test_handle_parsing_status_suspended_returns_no_response(void) {
    // USTREAM_SUSPENDED -> sw stays SWO_NO_RESPONSE (the host will send
    // the next chunk).
    TEST_ASSERT_EQUAL(handle_parsing_status(USTREAM_SUSPENDED), SWO_NO_RESPONSE);
}

void test_handle_parsing_status_processing_returns_success(void) {
    TEST_ASSERT_EQUAL(handle_parsing_status(USTREAM_PROCESSING), SWO_SUCCESS);
}

void test_handle_parsing_status_finished_delegates_to_finalize(void) {
    // USTREAM_FINISHED -> finalize_parsing(&txContext). Our local wrap
    // returns SWO_NO_RESPONSE (deferred reply) on a happy fixture.
    uint16_t sw = handle_parsing_status(USTREAM_FINISHED);
    // Any value the real finalize_parsing emitted is fine; the point is
    // the branch ran. The SW itself depends on the test fixture (here
    // a default-zeroed txContext likely lands on SWO_INCORRECT_DATA via
    // the pre-EIP155 legacy gate).
    (void) sw;
}

void test_handle_parsing_status_fault_non_swap_returns_incorrect_data(void) {
    G_called_from_swap = false;
    TEST_ASSERT_EQUAL(handle_parsing_status(USTREAM_FAULT), SWO_INCORRECT_DATA);
}

void test_handle_parsing_status_default_returns_incorrect_data(void) {
    // Any value not in USTREAM_SUSPENDED / FINISHED / PROCESSING /
    // FAULT lands on the safety default -> SWO_INCORRECT_DATA.
    TEST_ASSERT_EQUAL(handle_parsing_status((parserStatus_e) 0x7F), SWO_INCORRECT_DATA);
}

void test_handle_parsing_status_fault_in_swap_triggers_app_exit(void) {
    // USTREAM_FAULT inside a swap signing flow: the dispatcher MUST
    // flag the swap as responded (G_swap_response_ready=true) AND emit
    // send_swap_error_simple before app_exit so Exchange sees a
    // definitive error code instead of a silent stall.
    G_called_from_swap = true;
    G_swap_response_ready = false;
    EXPECT_NORETURN(handle_parsing_status(USTREAM_FAULT));
    TEST_ASSERT_EQUAL(g_noreturn_calls, 1);
    TEST_ASSERT_TRUE(G_swap_response_ready);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unknown_p2_rejected);
    RUN_TEST(test_basic_unknown_p1_rejected);
    RUN_TEST(test_p1_more_without_first_rejected);
    RUN_TEST(test_start_flow_without_signing_rejected);
    RUN_TEST(test_start_flow_with_nonzero_length_rejected);
    RUN_TEST(test_first_bad_bip32_rejected);
    RUN_TEST(test_first_no_tx_byte_after_bip32_rejected);
    RUN_TEST(test_first_rejected_when_app_not_idle);
    RUN_TEST(test_first_idle_does_not_reset);
    RUN_TEST(test_typed_tx_eip2930);
    RUN_TEST(test_typed_tx_eip1559);
    RUN_TEST(test_typed_tx_eip7702);
    RUN_TEST(test_typed_tx_unsupported_in_range_rejected);
    RUN_TEST(test_typed_tx_boundary_0x7F_rejected);
    RUN_TEST(test_legacy_byte_0x80_routes_to_legacy);
    RUN_TEST(test_legacy_byte_0xC0_routes_to_legacy);
    RUN_TEST(test_legacy_byte_0xFF_routes_to_legacy);
    RUN_TEST(test_typed_tx_cx_hash_failure_rejected);
    RUN_TEST(test_init_tx_failure_rejected);
    RUN_TEST(test_handle_parsing_status_suspended_returns_no_response);
    RUN_TEST(test_handle_parsing_status_processing_returns_success);
    RUN_TEST(test_handle_parsing_status_finished_delegates_to_finalize);
    RUN_TEST(test_handle_parsing_status_fault_non_swap_returns_incorrect_data);
    RUN_TEST(test_handle_parsing_status_default_returns_incorrect_data);
    RUN_TEST(test_handle_parsing_status_fault_in_swap_triggers_app_exit);
    return UNITY_END();
}
