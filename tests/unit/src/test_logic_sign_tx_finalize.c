/**
 * @file test_logic_sign_tx_finalize.c
 * @brief Unit tests for finalize_parsing() and its (static) helper
 *        finalize_parsing_helper() in src/features/sign_tx/logic_sign_tx.c.
 *
 * finalize_parsing is the gate every signed Ethereum transaction passes
 * through after RLP parsing completes: it verifies the chain-id binding,
 * commits the transaction hash, finalises the plugin (if any), formats
 * the on-screen fields, and starts the review UI. A wrong branch here
 * is the difference between the user signing what they reviewed and the
 * user signing what an attacker shaped.
 *
 * The helper is `static __attribute__((noinline))` so we can't linker-
 * wrap it -- the tests exercise the real body and pin the externally-
 * observable side-effects (return SW, strings written, app_exit
 * triggered, ux_approve_tx fired) by controlling every SDK leaf and
 * every app-side helper through a wrap or a global.
 *
 * The noreturn helpers (app_exit, app_quit, send_swap_error_simple) are
 * caught with a setjmp/longjmp pair so the test process survives.
 */

#include "unity.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "common_utils.h"
#include "feature_sign_tx.h"
#include "eth_ustream.h"
#include "tx_ctx.h"
#include "eth_swap_utils.h"
#include "wraps.h"
#include "Mocknetwork.h"

// =============================================================================
// Network mock state
// =============================================================================

static uint64_t s_tx_chain_id = 1;
static uint64_t get_tx_chain_id_stub(int cmock_num_calls) {
    (void) cmock_num_calls;
    return s_tx_chain_id;
}

static const char *s_displayable_ticker = "ETH";
static const char *get_displayable_ticker_stub(const uint64_t *chain_id,
                                               const chain_config_t *chain_cfg,
                                               bool dynamic,
                                               int cmock_num_calls) {
    (void) chain_id;
    (void) chain_cfg;
    (void) dynamic;
    (void) cmock_num_calls;
    return s_displayable_ticker;
}

// =============================================================================
// Wraps + globals
// =============================================================================
// The wraps below are unique to this test (plugin / swap / IO seph
// helpers + cx_hash_no_throw) -- no central default would carry the
// same intent.

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

// eth_plugin_call: drive the per-method return value via a small table.
// The two methods finalize_parsing_helper consults are FINALIZE and
// PROVIDE_INFO. Other methods aren't reached on the paths we test.
static int g_plugin_call_finalize_ret = ETH_PLUGIN_RESULT_FALLBACK;
static int g_plugin_call_provide_info_ret = ETH_PLUGIN_RESULT_SUCCESSFUL;
static int g_plugin_call_calls = 0;
int eth_plugin_call(int method, void *params) {
    (void) params;
    g_plugin_call_calls++;
    if (method == ETH_PLUGIN_FINALIZE) {
        return g_plugin_call_finalize_ret;
    }
    if (method == ETH_PLUGIN_PROVIDE_INFO) {
        return g_plugin_call_provide_info_ret;
    }
    return ETH_PLUGIN_RESULT_FALLBACK;
}

static int g_send_status_calls = 0;
static uint16_t g_send_status_last_sw = 0;
uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    (void) tx;
    (void) reset;
    (void) idle;
    g_send_status_calls++;
    g_send_status_last_sw = sw;
    return 0;
}

static int g_touch_tx_ok_calls = 0;
uint32_t io_seproxyhal_touch_tx_ok(const void *e) {
    (void) e;
    g_touch_tx_ok_calls++;
    return 0;
}

static int g_blind_signing_calls = 0;
void ui_error_blind_signing(void) {
    g_blind_signing_calls++;
}

static int g_hold_error_screen_calls = 0;
void ui_hold_error_screen(void) {
    g_hold_error_screen_calls++;
}

static int g_ux_approve_calls = 0;
static bool g_ux_approve_last_from_plugin = false;
uint16_t ux_approve_tx(bool fromPlugin) {
    g_ux_approve_calls++;
    g_ux_approve_last_from_plugin = fromPlugin;
    return 0;
}

static bool g_swap_check_destination_ret = true;
bool swap_check_destination(const char *destination) {
    (void) destination;
    return g_swap_check_destination_ret;
}

static bool g_swap_check_value_ret = true;
bool swap_check_value(const char *formatted_value,
                      const uint8_t *raw_value,
                      uint8_t raw_value_len) {
    (void) formatted_value;
    (void) raw_value;
    (void) raw_value_len;
    return g_swap_check_value_ret;
}

static bool g_swap_check_fee_ret = true;
bool swap_check_fee(const char *fee) {
    (void) fee;
    return g_swap_check_fee_ret;
}

// mem_utils_free_and_null is reused from app_globals.c default (free). The
// wrap here just no-ops the pointer so tests stay deterministic.
void mem_utils_free_and_null(void **ptr_storage, const char *file, int line) {
    (void) file;
    (void) line;
    if (ptr_storage != NULL) {
        *ptr_storage = NULL;
    }
}

// custom_processor wraps. The default lets the plugin path run "as
// installed"; tests that need a specific outcome flip the *_ret global.
static eth_plugin_result_t g_perform_init_ret = ETH_PLUGIN_RESULT_UNAVAILABLE;
eth_plugin_result_t eth_plugin_perform_init(uint8_t *contract_address, void *init) {
    (void) contract_address;
    (void) init;
    return g_perform_init_ret;
}

static bool g_copy_tx_data_ret = true;
static uint8_t g_copy_tx_data_fill = 0xAA;
// Optional override: if g_copy_tx_data_pattern_len > 0, the wrap copies
// the first min(length, pattern_len) bytes of g_copy_tx_data_pattern
// into `out` instead of using the flat fill byte. Tests that need a
// specific byte layout (e.g. some 8-byte parameter chunks all-zero so
// split_binary_parameter_part's zero branch fires) set the pattern.
static const uint8_t *g_copy_tx_data_pattern = NULL;
static size_t g_copy_tx_data_pattern_len = 0;
bool copy_tx_data(txContext_t *context, uint8_t *out, uint32_t length) {
    (void) context;
    if (g_copy_tx_data_ret && out != NULL) {
        if (g_copy_tx_data_pattern && g_copy_tx_data_pattern_len > 0) {
            size_t n = length < g_copy_tx_data_pattern_len ? length : g_copy_tx_data_pattern_len;
            memcpy(out, g_copy_tx_data_pattern, n);
            if (n < length) memset(out + n, g_copy_tx_data_fill, length - n);
        } else {
            memset(out, g_copy_tx_data_fill, length);
        }
    }
    return g_copy_tx_data_ret;
}

static int g_ui_confirm_selector_calls = 0;
void ui_confirm_selector(void) {
    g_ui_confirm_selector_calls++;
}

static int g_ui_confirm_parameter_calls = 0;
void ui_confirm_parameter(void) {
    g_ui_confirm_parameter_calls++;
}

// Strong overrides for the calldata-store lookup helpers. Tests flip
// g_root_calldata_ret / g_calldata_selector_ret to drive the
// store_calldata post-helper branches. Default behaviour matches the
// WEAK stub in mock.c (both return NULL).
static s_calldata *g_root_calldata_ret = NULL;
static const uint8_t *g_calldata_selector_ret = NULL;
s_calldata *get_root_calldata(void) {
    return g_root_calldata_ret;
}
const uint8_t *calldata_get_selector(const s_calldata *node) {
    (void) node;
    return g_calldata_selector_ret;
}

static bool s_amountToString_ret = true;
bool __wrap_amountToString(const uint8_t *amount,
                           uint8_t amount_len,
                           uint8_t decimals,
                           const char *ticker,
                           char *out_buffer,
                           size_t out_buffer_size) {
    (void) amount;
    (void) amount_len;
    (void) decimals;
    (void) ticker;
    if (s_amountToString_ret && out_buffer != NULL && out_buffer_size > 0) {
        strncpy(out_buffer, "1.5", out_buffer_size);
        out_buffer[out_buffer_size - 1] = '\0';
    }
    return s_amountToString_ret;
}

static bool s_getEthDisplayableAddress_ret = true;
bool __wrap_getEthDisplayableAddress(const uint8_t *in,
                                     char *out,
                                     size_t out_len,
                                     uint64_t chainId) {
    (void) in;
    (void) chainId;
    if (s_getEthDisplayableAddress_ret && out != NULL && out_len > 0) {
        strncpy(out, "0xdeadbeef", out_len);
        out[out_len - 1] = '\0';
    }
    return s_getEthDisplayableAddress_ret;
}

static uint16_t s_get_public_key_ret = 0x9000;
uint16_t get_public_key(uint8_t *out, uint8_t out_size) {
    if (s_get_public_key_ret == 0x9000 && out != NULL && out_size >= 20) {
        memset(out, 0xAB, 20);
    }
    return s_get_public_key_ret;
}

static bool s_get_network_as_string_ret = true;
static bool get_network_as_string_stub(char *out, size_t out_len, int cmock_num_calls) {
    (void) cmock_num_calls;
    if (s_get_network_as_string_ret && out != NULL && out_len > 0) {
        strncpy(out, "Ethereum", out_len);
        out[out_len - 1] = '\0';
    }
    return s_get_network_as_string_ret;
}

// =============================================================================
// Fixture
// =============================================================================

static txContext_t s_ctx;
static cx_sha3_t s_hash_ctx;

static void reset(void) {
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.txType = LEGACY;

    s_tx_chain_id = 1;
    s_displayable_ticker = "ETH";
    g_cx_hash_ret = CX_OK;
    s_get_public_key_ret = SWO_SUCCESS;
    s_getEthDisplayableAddress_ret = true;
    s_amountToString_ret = true;
    s_get_network_as_string_ret = true;
    g_plugin_call_finalize_ret = ETH_PLUGIN_RESULT_FALLBACK;
    g_plugin_call_provide_info_ret = ETH_PLUGIN_RESULT_SUCCESSFUL;
    g_plugin_call_calls = 0;
    g_send_status_calls = 0;
    g_send_status_last_sw = 0;
    g_touch_tx_ok_calls = 0;
    g_blind_signing_calls = 0;
    g_ux_approve_calls = 0;
    g_ux_approve_last_from_plugin = false;
    g_swap_check_destination_ret = true;
    g_swap_check_value_ret = true;
    g_swap_check_fee_ret = true;
    g_perform_init_ret = ETH_PLUGIN_RESULT_UNAVAILABLE;
    g_copy_tx_data_ret = true;
    g_copy_tx_data_fill = 0xAA;
    g_copy_tx_data_pattern = NULL;
    g_copy_tx_data_pattern_len = 0;
    g_ui_confirm_selector_calls = 0;
    g_ui_confirm_parameter_calls = 0;
    g_root_calldata_ret = NULL;
    g_calldata_selector_ret = NULL;

    // Satisfy the swap chain-ID gate by default; swap tests were written
    // before it existed
    G_swap_expected_chain_id = s_tx_chain_id;
    G_swap_tx_had_calldata = false;
    G_swap_calldata_validated = false;

    g_chainConfig.chain_id = 1;

    memset(&tmpContent, 0, sizeof(tmpContent));
    memset(&tmpCtx, 0, sizeof(tmpCtx));
    memset(&dataContext, 0, sizeof(dataContext));
    memset(&strings, 0, sizeof(strings));
    pluginType = PLUGIN_TYPE_NONE;
    G_called_from_swap = false;
    G_swap_mode = SWAP_MODE_STANDARD;
    G_swap_response_ready = false;
    G_swap_checked = false;
    g_n_storage_writable.dataAllowed = true;
    g_n_storage_writable.contractDetails = false;

    // Default: hash ctx allocated so most tests bypass the early
    // SWO_INSUFFICIENT_MEMORY exit. v == 1 so we're past EIP-155.
    g_tx_hash_ctx = &s_hash_ctx;
    tmpContent.txContent.vLength = 1;
    tmpContent.txContent.v[0] = 0x25;

    // Plausible defaults for a 0-value transfer to a known address.
    tmpContent.txContent.destinationLength = ADDRESS_LENGTH;
    memset(tmpContent.txContent.destination, 0xCD, ADDRESS_LENGTH);
    tmpContent.txContent.value.length = 1;
    tmpContent.txContent.value.value[0] = 0;
    tmpContent.txContent.gasprice.length = 1;
    tmpContent.txContent.gasprice.value[0] = 1;
    tmpContent.txContent.startgas.length = 1;
    tmpContent.txContent.startgas.value[0] = 1;
    tmpContent.txContent.nonce.length = 1;
    tmpContent.txContent.nonce.value[0] = 0;

    g_noreturn_armed = false;
    g_noreturn_calls = 0;
}

// =============================================================================
// chain_id / pre-EIP155 gate
// =============================================================================

void test_chain_id_mismatch_non_mainnet_returns_no_response(void) {
    g_chainConfig.chain_id = 137;  // app expects Polygon
    s_tx_chain_id = 1;             // tx claims mainnet
    uint16_t sw = finalize_parsing(&s_ctx);
    TEST_ASSERT_EQUAL(sw, SWO_NO_RESPONSE);
    // The error helper also calls io_seproxyhal_send_status(SWO_INCORRECT_DATA).
    TEST_ASSERT_EQUAL(g_send_status_last_sw, SWO_INCORRECT_DATA);
}

void test_pre_eip155_legacy_rejected(void) {
    // LEGACY transaction with empty V field -> no chain_id binding ->
    // replay vulnerability across EVM chains. MUST refuse.
    s_ctx.txType = LEGACY;
    tmpContent.txContent.vLength = 0;
    uint16_t sw = finalize_parsing(&s_ctx);
    TEST_ASSERT_EQUAL(sw, SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_send_status_last_sw, SWO_INCORRECT_DATA);
}

void test_post_eip155_legacy_passes_chain_gate(void) {
    // Same setup but vLength != 0 -> the chain-binding gate lets it
    // through. The function proceeds and ultimately delegates to
    // start_signature_flow, whose return is ux_approve_tx's return
    // (stubbed to 0 = noreply / deferred).
    s_ctx.txType = LEGACY;
    tmpContent.txContent.vLength = 1;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), 0);
    TEST_ASSERT_EQUAL(g_ux_approve_calls, 1);
}

// =============================================================================
// Hash / pubkey / address-format failures
// =============================================================================

void test_null_hash_ctx_returns_insufficient_memory(void) {
    g_tx_hash_ctx = NULL;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_INSUFFICIENT_MEMORY);
}

void test_cx_hash_failure_propagates(void) {
    // finalize_parsing returns uint16_t, so the cx_err_t (32-bit) is
    // truncated to its low 16 bits.
    g_cx_hash_ret = CX_INVALID_PARAMETER;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), (uint16_t) CX_INVALID_PARAMETER);
}

void test_get_public_key_failure_propagates(void) {
    s_get_public_key_ret = 0x6700;  // generic SW_WRONG_LENGTH from the SDK
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), 0x6700);
}

void test_address_format_failure_returns_parameter_error(void) {
    // The sender address is formatted first; force it to fail.
    s_getEthDisplayableAddress_ret = false;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_PARAMETER_ERROR_NO_INFO);
}

// =============================================================================
// Plugin chain-id binding + finalize failure
// =============================================================================

void test_plugin_chain_id_mismatch_returns_no_response(void) {
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_SUCCESSFUL;
    dataContext.tokenContext.pluginChainId = 137;  // plugin registered for Polygon
    s_tx_chain_id = 1;                             // tx is on mainnet
    g_chainConfig.chain_id = 1;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_send_status_last_sw, SWO_INCORRECT_DATA);
}

void test_plugin_finalize_failure_returns_no_response(void) {
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_SUCCESSFUL;
    dataContext.tokenContext.pluginChainId = PLUGIN_CHAIN_ID_ANY;
    g_plugin_call_finalize_ret = ETH_PLUGIN_RESULT_ERROR;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_NO_RESPONSE);
}

// =============================================================================
// network-as-string failure (last gate before start_signature_flow)
// =============================================================================

void test_get_network_as_string_failure_returns_incorrect_data(void) {
    s_get_network_as_string_ret = false;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_INCORRECT_DATA);
}

// =============================================================================
// Blind-signing gate
// =============================================================================

void test_data_present_without_data_allowed_rejected(void) {
    tmpContent.txContent.dataPresent = true;
    g_n_storage_writable.dataAllowed = false;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_blind_signing_calls, 1);
}

// =============================================================================
// Happy path: no plugin, no swap -> start_signature_flow -> ux_approve_tx
// =============================================================================

void test_happy_path_no_plugin_no_swap_starts_review(void) {
    // finalize_parsing returns whatever start_signature_flow returns,
    // which is ux_approve_tx's return value (here stubbed to 0).
    pluginType = PLUGIN_TYPE_NONE;
    G_called_from_swap = false;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), 0);
    TEST_ASSERT_EQUAL(g_ux_approve_calls, 1);
    TEST_ASSERT_FALSE(g_ux_approve_last_from_plugin);
    // Display fields populated.
    TEST_ASSERT_EQUAL_STRING(strings.common.fromAddress, "0xdeadbeef");
    TEST_ASSERT_EQUAL_STRING(strings.common.toAddress, "0xdeadbeef");
    TEST_ASSERT_EQUAL_STRING(strings.common.fullAmount, "1.5");
}

void test_unavailable_plugin_reroutes_to_standard_review(void) {
    // pluginStatus stays UNAVAILABLE so the plugin branch inside the
    // helper is skipped, and the unavailable-plugin reroute then drops
    // pluginType to NONE: start_signature_flow must use the standard
    // review, not an empty plugin one.
    pluginType = PLUGIN_TYPE_EXTERNAL;
    G_called_from_swap = false;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), 0);
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_NONE);
    TEST_ASSERT_FALSE(g_ux_approve_last_from_plugin);
}

// =============================================================================
// store_calldata branches
// =============================================================================

void test_store_calldata_without_calldata_returns_incorrect_data(void) {
    s_ctx.store_calldata = true;
    // get_root_calldata stub returns NULL -> selector check fails.
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_INCORRECT_DATA);
    // UI MUST NOT fire on the store-only path.
    TEST_ASSERT_EQUAL(g_ux_approve_calls, 0);
}

// =============================================================================
// Swap branches
// =============================================================================

void test_swap_standard_success_calls_touch_tx_ok(void) {
    G_called_from_swap = true;
    G_swap_mode = SWAP_MODE_STANDARD;
    pluginType = PLUGIN_TYPE_NONE;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_touch_tx_ok_calls, 1);
    // No UI review in swap success mode.
    TEST_ASSERT_EQUAL(g_ux_approve_calls, 0);
}

void test_swap_double_sign_safety_triggers_app_quit(void) {
    G_called_from_swap = true;
    G_swap_response_ready = true;  // already-replied flag
    pluginType = PLUGIN_TYPE_NONE;
    // app_quit is *not* noreturn in production (shared_context.h declares
    // it `void app_quit(void)` -- callers wrap it in `while(1);` as a
    // defence in depth). The mock returns; we just count the call.
    g_noreturn_calls = 0;
    finalize_parsing(&s_ctx);
    TEST_ASSERT_EQUAL(g_noreturn_calls, 1);
}

void test_swap_unexpected_plugin_type_triggers_app_exit(void) {
    G_called_from_swap = true;
    G_swap_mode = SWAP_MODE_STANDARD;
    // pluginType is neither NONE, OLD_INTERNAL, nor SWAP_WITH_CALLDATA
    // -> swap-error + app_exit. Keep pluginStatus UNAVAILABLE so the
    // helper's plugin-finalize block is skipped (the swap-error gate
    // is what we want to hit).
    pluginType = PLUGIN_TYPE_EXTERNAL;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;
    EXPECT_NORETURN(finalize_parsing(&s_ctx));
}

void test_swap_crosschain_wrong_mode_triggers_app_exit(void) {
    G_called_from_swap = true;
    // PENDING_CHECK is neither STANDARD nor CROSSCHAIN_SUCCESS at the
    // mode-validation gate -> swap-error + app_exit.
    G_swap_mode = SWAP_MODE_CROSSCHAIN_PENDING_CHECK;
    pluginType = PLUGIN_TYPE_NONE;
    EXPECT_NORETURN(finalize_parsing(&s_ctx));
}

// =============================================================================
// finalize_parsing_helper edge branches
// =============================================================================

void test_helper_amount_to_string_failure_returns_overflow(void) {
    // pluginType == NONE -> enter the destination/amount block. Make
    // amountToString fail; helper bails with EXCEPTION_OVERFLOW (the
    // displayed amount would be garbage otherwise).
    pluginType = PLUGIN_TYPE_NONE;
    s_amountToString_ret = false;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), EXCEPTION_OVERFLOW);
}

void test_helper_max_fee_to_string_failure_returns_incorrect_data(void) {
    // Push gasprice and gasLimit to (2^256 - 1) each so mul256
    // overflows; max_transaction_fee_to_string returns false and the
    // helper surfaces SWO_INCORRECT_DATA.
    pluginType = PLUGIN_TYPE_NONE;
    memset(&tmpContent.txContent.gasprice, 0xFF, sizeof(tmpContent.txContent.gasprice));
    tmpContent.txContent.gasprice.length = 32;
    memset(&tmpContent.txContent.startgas, 0xFF, sizeof(tmpContent.txContent.startgas));
    tmpContent.txContent.startgas.length = 32;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_INCORRECT_DATA);
}

// =============================================================================
// store_calldata post-helper branches
// =============================================================================

void test_store_calldata_with_selector_returns_success(void) {
    // get_root_calldata + calldata_get_selector both return non-NULL
    // -> finalize_parsing reports SUCCESS (the host can now reuse the
    // buffered calldata in a later signing chunk).
    s_ctx.store_calldata = true;
    static int s_dummy_calldata;
    static uint8_t s_dummy_selector[4] = {0xa9, 0x05, 0x9c, 0xbb};
    g_root_calldata_ret = (s_calldata *) &s_dummy_calldata;
    g_calldata_selector_ret = s_dummy_selector;
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), SWO_SUCCESS);
    // store_calldata path does NOT fire the review UI.
    TEST_ASSERT_EQUAL(g_ux_approve_calls, 0);
}

// =============================================================================
// Swap mode post-helper guards
// =============================================================================

void test_swap_old_internal_with_swap_unchecked_triggers_app_exit(void) {
    // pluginType == OLD_INTERNAL is in the swap-allow list, but the
    // helper only sets G_swap_checked for NONE / SWAP_WITH_CALLDATA --
    // so after the helper succeeds, the post-helper safety net catches
    // the missing check and triggers send_swap_error_simple + app_exit.
    G_called_from_swap = true;
    G_swap_mode = SWAP_MODE_STANDARD;
    pluginType = PLUGIN_TYPE_OLD_INTERNAL;
    EXPECT_NORETURN(finalize_parsing(&s_ctx));
}

void test_swap_standard_with_unvalidated_calldata_triggers_app_exit(void) {
    // The swap calldata gate: G_swap_tx_had_calldata is recorded while
    // parsing the RLP data field and nothing validated it against the
    // swap promise -> refuse with swap error + app_exit.
    G_called_from_swap = true;
    G_swap_mode = SWAP_MODE_STANDARD;
    pluginType = PLUGIN_TYPE_NONE;  // so helper sets G_swap_checked = true
    G_swap_tx_had_calldata = true;
    G_swap_calldata_validated = false;
    g_n_storage_writable.dataAllowed = true;  // bypass the earlier blind-signing gate
    EXPECT_NORETURN(finalize_parsing(&s_ctx));
}

// =============================================================================
// address_to_string -- "Contract" placeholder when no destination
// =============================================================================
// destinationLength == 0 means contract-creation; the formatter must
// write "Contract" into strings.common.toAddress instead of calling
// getEthDisplayableAddress. The bug we guard against is the inverse:
// silently calling getEthDisplayableAddress on a NULL-length buffer.

void test_contract_creation_writes_contract_placeholder(void) {
    pluginType = PLUGIN_TYPE_NONE;
    G_called_from_swap = false;
    tmpContent.txContent.destinationLength = 0;  // contract creation
    TEST_ASSERT_EQUAL(finalize_parsing(&s_ctx), 0);
    TEST_ASSERT_EQUAL_STRING(strings.common.toAddress, "Contract");
}

// =============================================================================
// raw_fee_to_string -- ticker-too-long short-circuit
// =============================================================================
// raw_fee_to_string is `static`; reach it through max_transaction_fee_
// to_string. The "(strlen + 1 + ticker_len + 1) > out_buffer_size"
// gate fires when the fee buffer is small AND the ticker is long.
// A regression here would silently emit a fee string with NO ticker --
// the user sees the number on screen but cannot tell which chain the
// fee is denominated in.

void test_raw_fee_to_string_long_ticker_fails_closed(void) {
    // adjustDecimals needs at least (srcLength + 2) bytes to write the
    // raw decimal form ("1.000000000000000000" then trim). For
    // 1e18 wei (srcLength=19) the minimum is 21 bytes -- so use a
    // 22-byte buffer to let adjustDecimals through, then a ticker long
    // enough to overflow the remaining space (1 + 1 + ticker_len + 1
    // > 22 -> ticker_len > 19).
    txInt256_t gp = {.length = 8, .value = {0x0D, 0xE0, 0xB6, 0xB3, 0xA7, 0x64, 0x00, 0x00}};
    txInt256_t gl = {.length = 1, .value = {0x01}};
    s_displayable_ticker = "ABCDEFGHIJKLMNOPQRST";  // 20 chars
    char buf[22] = {0};
    bool ok = max_transaction_fee_to_string(&gp, &gl, buf, sizeof(buf));
    // Fee formatting failures propagate: a fee whose ticker does not fit
    // is rejected rather than shown as a bare number the user cannot
    // attribute to a chain.
    TEST_ASSERT_FALSE(ok);
}

// =============================================================================
// custom_processor -- per-chunk RLP DATA dispatcher
// =============================================================================
// custom_processor is what eth_ustream calls for every RLP DATA chunk
// during the parse. It decides whether to:
//   - skip the chunk entirely (CUSTOM_NOT_HANDLED, no calldata handling)
//   - dispatch to a plugin (CUSTOM_HANDLED / CUSTOM_SUSPENDED)
//   - reject the transaction (CUSTOM_FAULT, e.g. blind-signing forbidden)
//
// A bug here either skips a plugin parameter (the review shows partial
// info) or accepts data that should have been rejected as blind
// signing. The fixture sets up a "valid first chunk on LEGACY RLP DATA"
// baseline; each test perturbs one field.

static void cp_setup_first_chunk(void) {
    // Default: LEGACY tx, first chunk of an 8-byte data field, plugin
    // unavailable, no contractDetails, data allowed.
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.txType = LEGACY;
    s_ctx.currentField = LEGACY_RLP_DATA;
    s_ctx.currentFieldLength = 8;
    s_ctx.currentFieldPos = 0;
    s_ctx.commandLength = 8;
    s_ctx.store_calldata = false;
    // context->content is dereferenced on the first `dataPresent = true`
    // -- point it at the app-globals tmpContent so the test process
    // doesn't segfault.
    s_ctx.content = &tmpContent.txContent;
    tmpContent.txContent.destinationLength = ADDRESS_LENGTH;
    g_n_storage_writable.dataAllowed = true;
    g_n_storage_writable.contractDetails = false;
    G_called_from_swap = false;
}

// --- outer gates ---------------------------------------------------------

void test_cp_not_rlp_data_field_returns_not_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentField = LEGACY_RLP_DATA + 1;  // any non-DATA field
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
    // dataPresent MUST NOT be flipped on a non-DATA field.
    TEST_ASSERT_FALSE(tmpContent.txContent.dataPresent);
}

void test_cp_empty_field_returns_not_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldLength = 0;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
    TEST_ASSERT_FALSE(tmpContent.txContent.dataPresent);
}

void test_cp_new_contract_skips_plugin_dispatch(void) {
    cp_setup_first_chunk();
    tmpContent.txContent.destinationLength = 0;  // contract creation
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
    // dataPresent flips even though we don't dispatch -- the user must
    // still see "Contract creation: data attached".
    TEST_ASSERT_TRUE(tmpContent.txContent.dataPresent);
}

void test_cp_short_field_returns_not_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldLength = 3;  // shorter than CALLDATA_SELECTOR_SIZE
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
    TEST_ASSERT_TRUE(tmpContent.txContent.dataPresent);
}

// --- first chunk -- plugin path -----------------------------------------

void test_cp_first_chunk_short_command_returns_fault(void) {
    cp_setup_first_chunk();
    s_ctx.commandLength = 3;  // can't even read the selector -- corrupt frame
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_FAULT);
}

void test_cp_plugin_init_error_returns_fault(void) {
    cp_setup_first_chunk();
    g_n_storage_writable.contractDetails = false;  // trigger plugin path
    g_perform_init_ret = ETH_PLUGIN_RESULT_ERROR;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_FAULT);
}

void test_cp_plugin_success_selector_only_returns_not_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldLength = 4;  // only the selector, no params
    s_ctx.commandLength = 4;
    g_perform_init_ret = ETH_PLUGIN_RESULT_SUCCESSFUL;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
}

void test_cp_plugin_success_selector_copy_failure_returns_fault(void) {
    cp_setup_first_chunk();
    g_perform_init_ret = ETH_PLUGIN_RESULT_SUCCESSFUL;
    g_copy_tx_data_ret = false;  // selector copy fails
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_FAULT);
}

// --- first chunk -- no plugin / blind signing gate ----------------------

void test_cp_first_chunk_data_forbidden_returns_fault(void) {
    cp_setup_first_chunk();
    g_n_storage_writable.dataAllowed = false;  // blind signing forbidden
    g_hold_error_screen_calls = 0;
    // pluginStatus stays UNAVAILABLE -> we fall through to the no-plugin
    // path which checks dataAllowed.
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_FAULT);
    TEST_ASSERT_EQUAL(g_blind_signing_calls, 1);
    // The refusal screen must outlive the error the main loop answers with.
    TEST_ASSERT_EQUAL(g_hold_error_screen_calls, 1);
}

void test_cp_first_chunk_store_calldata_returns_not_handled(void) {
    cp_setup_first_chunk();
    s_ctx.store_calldata = true;  // host asked us to buffer the calldata
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
}

void test_cp_first_chunk_no_contract_details_returns_not_handled(void) {
    cp_setup_first_chunk();
    g_n_storage_writable.contractDetails = false;
    // pluginStatus stays UNAVAILABLE, dataAllowed=true -> falls to the
    // `store_calldata || !contractDetails` gate -> NOT_HANDLED.
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
}

// --- first chunk -- contractDetails on -> selector confirm UI -----------

void test_cp_first_chunk_contract_details_shows_selector(void) {
    cp_setup_first_chunk();
    g_n_storage_writable.contractDetails = true;  // user wants to see selector
    // perform_init NOT called when contractDetails is on AND
    // G_called_from_swap is false (early return guard). pluginStatus
    // stays UNAVAILABLE -> falls to the no-plugin path.
    // blockSize=4, copySize=4 -> SUSPENDED with ui_confirm_selector.
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_SUSPENDED);
    TEST_ASSERT_EQUAL(g_ui_confirm_selector_calls, 1);
    TEST_ASSERT_EQUAL(g_ui_confirm_parameter_calls, 0);
}

// --- continuation chunks (currentFieldPos > 0) --------------------------

void test_cp_continuation_store_calldata_returns_not_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldPos = 4;  // already past the selector
    s_ctx.currentFieldLength = 36;
    s_ctx.commandLength = 32;
    s_ctx.store_calldata = true;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
}

void test_cp_continuation_no_plugin_no_details_returns_not_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldPos = 4;
    s_ctx.currentFieldLength = 36;
    s_ctx.commandLength = 32;
    g_n_storage_writable.contractDetails = false;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_NOT_HANDLED);
}

void test_cp_continuation_plugin_provide_param_success_returns_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldPos = 4;
    s_ctx.currentFieldLength = 4 + CALLDATA_CHUNK_SIZE;  // selector + 1 param
    s_ctx.commandLength = CALLDATA_CHUNK_SIZE;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_SUCCESSFUL;
    // eth_plugin_call(PROVIDE_PARAMETER) defaults to FALLBACK which is
    // > ETH_PLUGIN_RESULT_ERROR; the source uses `if (!eth_plugin_call(...))`
    // so any non-zero return is "ok". g_plugin_call_finalize_ret is the
    // FINALIZE method; PROVIDE_PARAMETER isn't routed through it.
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_HANDLED);
    // fieldIndex bumps after a successful PROVIDE_PARAMETER round.
    TEST_ASSERT_EQUAL(dataContext.tokenContext.fieldIndex, 1);
}

void test_cp_continuation_plugin_partial_block_returns_handled(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldPos = 4;
    s_ctx.currentFieldLength = 4 + CALLDATA_CHUNK_SIZE;
    s_ctx.commandLength = 10;  // less than blockSize -> partial copy
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_SUCCESSFUL;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_HANDLED);
    // No PROVIDE_PARAMETER on partial block; fieldOffset advanced.
    TEST_ASSERT_EQUAL(dataContext.tokenContext.fieldOffset, 10);
}

void test_cp_continuation_copy_failure_returns_fault(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldPos = 4;
    s_ctx.currentFieldLength = 4 + CALLDATA_CHUNK_SIZE;
    s_ctx.commandLength = CALLDATA_CHUNK_SIZE;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_SUCCESSFUL;
    g_copy_tx_data_ret = false;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_FAULT);
}

// --- continuation chunk -- split_binary_parameter_part zero branch ---

void test_cp_continuation_zero_param_segments_use_short_form(void) {
    // contractDetails=true + continuation chunk + full 32-byte block ->
    // ui_confirm_parameter path, which calls split_binary_parameter_part
    // four times on consecutive 8-byte segments. When a segment is all
    // zeros the helper writes "00" instead of "0000000000000000".
    // Build a pattern with one zero segment (the second 8 bytes) so
    // that branch fires.
    cp_setup_first_chunk();
    s_ctx.currentFieldPos = 4;
    s_ctx.currentFieldLength = 4 + CALLDATA_CHUNK_SIZE;
    s_ctx.commandLength = CALLDATA_CHUNK_SIZE;
    g_n_storage_writable.contractDetails = true;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;
    static const uint8_t pattern[CALLDATA_CHUNK_SIZE] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,  // segment 0: non-zero
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // segment 1: all-zero
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    };
    g_copy_tx_data_pattern = pattern;
    g_copy_tx_data_pattern_len = sizeof(pattern);
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_SUSPENDED);
    TEST_ASSERT_EQUAL(g_ui_confirm_parameter_calls, 1);
    // The formatted output: segment0:00:segment2:segment3 -- segment 1
    // collapses to "00" thanks to the zero-only branch.
    TEST_ASSERT_NOT_NULL(strstr(strings.tmp.tmp, "1122334455667788:00:"));
}

// --- continuation chunk -- contractDetails, full block -> parameter UI ---

void test_cp_continuation_contract_details_shows_parameter(void) {
    cp_setup_first_chunk();
    s_ctx.currentFieldPos = 4;
    s_ctx.currentFieldLength = 4 + CALLDATA_CHUNK_SIZE;
    s_ctx.commandLength = CALLDATA_CHUNK_SIZE;
    g_n_storage_writable.contractDetails = true;
    dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;
    TEST_ASSERT_EQUAL(custom_processor(&s_ctx), CUSTOM_SUSPENDED);
    TEST_ASSERT_EQUAL(g_ui_confirm_parameter_calls, 1);
    TEST_ASSERT_EQUAL(g_ui_confirm_selector_calls, 0);
}

// =============================================================================
// Tests already pinned in test_logic_sign_tx_fee.c remain there; this
// file deliberately doesn't re-cover max_transaction_fee_to_string.
// =============================================================================

void setUp(void) {
    Mocknetwork_Init();
    get_tx_chain_id_StubWithCallback(get_tx_chain_id_stub);
    get_displayable_ticker_StubWithCallback(get_displayable_ticker_stub);
    get_network_as_string_StubWithCallback(get_network_as_string_stub);
    reset();
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_chain_id_mismatch_non_mainnet_returns_no_response);
    RUN_TEST(test_pre_eip155_legacy_rejected);
    RUN_TEST(test_post_eip155_legacy_passes_chain_gate);
    RUN_TEST(test_null_hash_ctx_returns_insufficient_memory);
    RUN_TEST(test_cx_hash_failure_propagates);
    RUN_TEST(test_get_public_key_failure_propagates);
    RUN_TEST(test_address_format_failure_returns_parameter_error);
    RUN_TEST(test_plugin_chain_id_mismatch_returns_no_response);
    RUN_TEST(test_plugin_finalize_failure_returns_no_response);
    RUN_TEST(test_get_network_as_string_failure_returns_incorrect_data);
    RUN_TEST(test_data_present_without_data_allowed_rejected);
    RUN_TEST(test_happy_path_no_plugin_no_swap_starts_review);
    RUN_TEST(test_unavailable_plugin_reroutes_to_standard_review);
    RUN_TEST(test_store_calldata_without_calldata_returns_incorrect_data);
    RUN_TEST(test_swap_standard_success_calls_touch_tx_ok);
    RUN_TEST(test_swap_double_sign_safety_triggers_app_quit);
    RUN_TEST(test_swap_unexpected_plugin_type_triggers_app_exit);
    RUN_TEST(test_swap_crosschain_wrong_mode_triggers_app_exit);
    RUN_TEST(test_helper_amount_to_string_failure_returns_overflow);
    RUN_TEST(test_helper_max_fee_to_string_failure_returns_incorrect_data);
    RUN_TEST(test_store_calldata_with_selector_returns_success);
    RUN_TEST(test_swap_old_internal_with_swap_unchecked_triggers_app_exit);
    RUN_TEST(test_swap_standard_with_unvalidated_calldata_triggers_app_exit);
    RUN_TEST(test_contract_creation_writes_contract_placeholder);
    RUN_TEST(test_raw_fee_to_string_long_ticker_fails_closed);
    RUN_TEST(test_cp_not_rlp_data_field_returns_not_handled);
    RUN_TEST(test_cp_empty_field_returns_not_handled);
    RUN_TEST(test_cp_new_contract_skips_plugin_dispatch);
    RUN_TEST(test_cp_short_field_returns_not_handled);
    RUN_TEST(test_cp_first_chunk_short_command_returns_fault);
    RUN_TEST(test_cp_plugin_init_error_returns_fault);
    RUN_TEST(test_cp_plugin_success_selector_only_returns_not_handled);
    RUN_TEST(test_cp_plugin_success_selector_copy_failure_returns_fault);
    RUN_TEST(test_cp_first_chunk_data_forbidden_returns_fault);
    RUN_TEST(test_cp_first_chunk_store_calldata_returns_not_handled);
    RUN_TEST(test_cp_first_chunk_no_contract_details_returns_not_handled);
    RUN_TEST(test_cp_first_chunk_contract_details_shows_selector);
    RUN_TEST(test_cp_continuation_store_calldata_returns_not_handled);
    RUN_TEST(test_cp_continuation_no_plugin_no_details_returns_not_handled);
    RUN_TEST(test_cp_continuation_plugin_provide_param_success_returns_handled);
    RUN_TEST(test_cp_continuation_plugin_partial_block_returns_handled);
    RUN_TEST(test_cp_continuation_copy_failure_returns_fault);
    RUN_TEST(test_cp_continuation_zero_param_segments_use_short_form);
    RUN_TEST(test_cp_continuation_contract_details_shows_parameter);
    return UNITY_END();
}
