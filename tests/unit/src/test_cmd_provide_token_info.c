/**
 * @file test_cmd_provide_token_info.c
 * @brief Unit tests for the ERC-20 token metadata descriptor at
 *        src/features/provide_erc20_token_information/cmd_provide_token_info.c.
 *
 * The host pairs (ticker, decimals, contract_address, chain_id) with
 * a Ledger signature; once accepted the device shows that ticker /
 * decimal when displaying token transfers. A bug in the parser or
 * the signature gate would let an attacker pair an arbitrary ticker
 * with any contract address — so the user sees "USDC" while signing
 * a transfer to a phishing token.
 *
 * Two wire formats are dispatched on P1:
 *   - P1=0 : legacy fixed layout (ticker_len, ticker, address, decimals,
 *            chain_id, signature),
 *   - P1=1 : TLV with a TUID sub-payload (chain_id + address).
 * P2 toggles "first chunk" for the chunked TLV path.
 *
 * This suite pins:
 *   - P1 / P2 dispatcher rejects unknown combinations,
 *   - the legacy path's truncation guards on each field,
 *   - the decimals > 255 guard (the field is wire-encoded as 4 bytes),
 *   - the unsupported-chain short-circuit happens before storage,
 *   - check_signature_with_pubkey failure leaves no descriptor stored,
 *   - the happy paths return SWO_SUCCESS, bump *tx, and write the
 *     storage index into G_io_tx_buffer[0],
 *   - the TLV path rejects a tlv_use_case parser failure and a
 *     coin_type mismatch.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "apdu_constants.h"
#include "token_info.h"
#include "shared_context.h"
#include "tlv_apdu.h"
#include "tlv_use_case_dynamic_descriptor.h"
#include "Mockpublic_keys.h"

// =============================================================================
// Globals required by linked translation units
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// check_signature_with_pubkey is a CMock-generated mock.
// Control its return value via s_sig_check_ret below.
static bool s_sig_check_ret = true;
static bool sig_check_stub(uint8_t *buffer,
                           const uint8_t bufLen,
                           const uint8_t *PubKey,
                           const uint8_t keyLen,
                           const uint8_t keyUsageExp,
                           const uint8_t *signature,
                           const uint8_t sigLen,
                           int num_calls) {
    (void) buffer;
    (void) bufLen;
    (void) PubKey;
    (void) keyLen;
    (void) keyUsageExp;
    (void) signature;
    (void) sigLen;
    (void) num_calls;
    return s_sig_check_ret;
}

static bool g_chain_compatible = true;
bool chain_id_is_signable(uint64_t chain_id) {
    (void) chain_id;
    return g_chain_compatible;
}

// Drive the TLV use-case output. The handler reads coin_type, ticker,
// magnitude, and a TUID buffer that the legacy ERC-20 helper feeds to
// the local sub-TLV parser parse_dynamic_token_tuid.
static tlv_dynamic_descriptor_status_t g_tlv_uc_ret = TLV_DYNAMIC_DESCRIPTOR_SUCCESS;
static uint8_t g_tlv_uc_coin_type = 60;
static const char *g_tlv_uc_ticker = "USDC";
static uint8_t g_tlv_uc_magnitude = 6;
static const uint8_t *g_tlv_uc_tuid_ptr = NULL;
static size_t g_tlv_uc_tuid_size = 0;

tlv_dynamic_descriptor_status_t tlv_use_case_dynamic_descriptor(const buffer_t *payload,
                                                                tlv_dynamic_descriptor_out_t *out) {
    (void) payload;
    if (g_tlv_uc_ret != TLV_DYNAMIC_DESCRIPTOR_SUCCESS) {
        return g_tlv_uc_ret;
    }
    out->version = 1;
    out->coin_type = g_tlv_uc_coin_type;
    out->magnitude = g_tlv_uc_magnitude;
    strncpy(out->ticker, g_tlv_uc_ticker, sizeof(out->ticker) - 1);
    out->ticker[sizeof(out->ticker) - 1] = '\0';
    out->TUID.ptr = (uint8_t *) g_tlv_uc_tuid_ptr;
    out->TUID.size = g_tlv_uc_tuid_size;
    return TLV_DYNAMIC_DESCRIPTOR_SUCCESS;
}

// =============================================================================
// Legacy P1=0 payload builder
// =============================================================================
//
//   [ticker_len:1] [ticker:N] [address:20] [decimals:4 BE] [chain_id:4 BE] [sig:M]
//

typedef struct {
    uint8_t ticker_len;
    const char *ticker;
    uint8_t address_byte;
    uint32_t decimals;
    uint32_t chain_id;
    uint8_t sig_len;
    bool include_signature;
    int8_t truncate_at;  // 0 = no ticker_len, 1 = no ticker, ...
} s_legacy_opts;

static s_legacy_opts default_legacy(void) {
    s_legacy_opts o = {.ticker_len = 4,
                       .ticker = "USDC",
                       .address_byte = 0xAA,
                       .decimals = 6,
                       .chain_id = 1,
                       .sig_len = 64,
                       .include_signature = true,
                       .truncate_at = -1};
    return o;
}

static size_t build_legacy(uint8_t *out, size_t out_size, const s_legacy_opts *opts) {
    size_t off = 0;
    out[off++] = opts->ticker_len;
    if (opts->truncate_at == 0) return off;
    if (opts->ticker != NULL) {
        for (uint8_t i = 0; i < opts->ticker_len; i++) {
            out[off++] = (uint8_t) opts->ticker[i];
        }
    } else {
        memset(out + off, 0, opts->ticker_len);
        off += opts->ticker_len;
    }
    if (opts->truncate_at == 1) return off;
    memset(out + off, opts->address_byte, 20);
    off += 20;
    if (opts->truncate_at == 2) return off;
    for (int i = 3; i >= 0; i--) {
        out[off++] = (uint8_t) (opts->decimals >> (8 * i));
    }
    if (opts->truncate_at == 3) return off;
    for (int i = 3; i >= 0; i--) {
        out[off++] = (uint8_t) (opts->chain_id >> (8 * i));
    }
    if (opts->truncate_at == 4) return off;
    if (opts->include_signature) {
        for (uint8_t i = 0; i < opts->sig_len; i++) {
            out[off++] = 0x42;
        }
    }
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    clear_token_infos();
    s_sig_check_ret = true;
    g_chain_compatible = true;
    g_tlv_uc_ret = TLV_DYNAMIC_DESCRIPTOR_SUCCESS;
    g_tlv_uc_coin_type = 60;
    g_tlv_uc_ticker = "USDC";
    g_tlv_uc_magnitude = 6;
    g_tlv_uc_tuid_ptr = NULL;
    g_tlv_uc_tuid_size = 0;
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
    tlv_from_apdu(false, 0, NULL, NULL);
}

// =============================================================================
// Tests — P1 / P2 dispatcher
// =============================================================================

void test_unknown_p1_rejected(void) {
    uint8_t apdu[100];
    s_legacy_opts opts = default_legacy();
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(/*p1=*/2, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_P1_P2);
}

void test_legacy_p2_nonzero_rejected(void) {
    uint8_t apdu[100];
    s_legacy_opts opts = default_legacy();
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, /*p2=*/1, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_P1_P2);
}

// =============================================================================
// Tests — P1=0 legacy path
// =============================================================================

void test_legacy_happy_path(void) {
    uint8_t apdu[200];
    s_legacy_opts opts = default_legacy();
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 1);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);

    uint8_t addr[20];
    memset(addr, 0xAA, sizeof(addr));
    uint64_t chain = 1;
    const s_token_info *got = get_matching_token_info(&chain, addr);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_MEMORY(got->ticker, "USDC", 4);
    TEST_ASSERT_EQUAL(got->decimals, 6);
    TEST_ASSERT_EQUAL(got->chain_id, 1);
}

void test_legacy_truncated_missing_ticker(void) {
    uint8_t apdu[100];
    s_legacy_opts opts = default_legacy();
    opts.truncate_at = 0;  // emit only ticker_len
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_legacy_truncated_missing_address(void) {
    uint8_t apdu[100];
    s_legacy_opts opts = default_legacy();
    opts.truncate_at = 1;
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_legacy_truncated_missing_decimals(void) {
    uint8_t apdu[100];
    s_legacy_opts opts = default_legacy();
    opts.truncate_at = 2;
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_legacy_truncated_missing_chain_id(void) {
    uint8_t apdu[100];
    s_legacy_opts opts = default_legacy();
    opts.truncate_at = 3;
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_legacy_missing_signature_rejected(void) {
    uint8_t apdu[100];
    s_legacy_opts opts = default_legacy();
    opts.include_signature = false;
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    // The source bails *before* calling cx_hash_sha256 / check_signature
    // when offset >= lc — no oracle.
    TEST_ASSERT_EQUAL(check_signature_with_pubkey_CallCount(), 0);
}

void test_legacy_decimals_above_uint8_rejected(void) {
    uint8_t apdu[200];
    s_legacy_opts opts = default_legacy();
    opts.decimals = 0x100;  // > UINT8_MAX
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_legacy_unsupported_chain_id_rejected(void) {
    g_chain_compatible = false;
    uint8_t apdu[200];
    s_legacy_opts opts = default_legacy();
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_legacy_signature_failure_does_not_store(void) {
    s_sig_check_ret = false;
    uint8_t apdu[200];
    s_legacy_opts opts = default_legacy();
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    uint8_t addr[20];
    memset(addr, 0xAA, sizeof(addr));
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_token_info(&chain, addr));
}

void test_legacy_ticker_too_long_rejected(void) {
    // MAX_TICKER_LEN includes the trailing NUL slot (50 chars + '\0').
    // ticker_len = MAX_TICKER_LEN (51) breaks the `+ 1 > sizeof(...)` guard
    // in erc20_token_info_common.
    uint8_t apdu[200];
    s_legacy_opts opts = default_legacy();
    opts.ticker_len = MAX_TICKER_LEN;
    opts.ticker = NULL;  // zero-fill
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_legacy_duplicate_returns_same_index(void) {
    uint8_t apdu[200];
    s_legacy_opts opts = default_legacy();
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);

    unsigned int tx = 0;
    (void) handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);

    tx = 0;
    memset(G_io_tx_buffer, 0xCC, 4);
    uint16_t sw = handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);
}

void test_legacy_second_descriptor_gets_next_index(void) {
    uint8_t apdu[200];

    s_legacy_opts opts = default_legacy();
    size_t len = build_legacy(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    (void) handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);

    opts.address_byte = 0xBB;
    opts.ticker = "USDT";
    len = build_legacy(apdu, sizeof(apdu), &opts);
    tx = 0;
    (void) handle_provide_erc20_token_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 1);
}

// =============================================================================
// Tests — P1=1 TLV path
// =============================================================================

// Build a minimal TUID sub-TLV with TAG_CHAIN_ID (0x23) and TAG_ADDRESS
// (0x22) so parse_dynamic_token_tuid succeeds.
static size_t build_tuid(uint8_t *out, size_t out_size, uint64_t chain_id, uint8_t address_byte) {
    size_t off = 0;
    out[off++] = 0x23;
    out[off++] = 1;
    out[off++] = (uint8_t) chain_id;
    out[off++] = 0x22;
    out[off++] = 20;
    memset(out + off, address_byte, 20);
    off += 20;
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

void test_tlv_use_case_failure_rejects(void) {
    g_tlv_uc_ret = TLV_DYNAMIC_DESCRIPTOR_PARSING_ERROR;
    uint8_t apdu[64] = {0};  // body ignored — TLV stub wins first
    // P1=1 chunked TLV path takes a length prefix on the first chunk.
    apdu[0] = 0;
    apdu[1] = 1;
    apdu[2] = 0xAA;
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(/*p1=*/1, P1_FIRST_CHUNK, 3, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_tlv_coin_type_mismatch_rejects(void) {
    g_tlv_uc_coin_type = 1;  // BTC, not ETH (60)
    uint8_t tuid[32];
    size_t tuid_len = build_tuid(tuid, sizeof(tuid), 1, 0xAA);
    g_tlv_uc_tuid_ptr = tuid;
    g_tlv_uc_tuid_size = tuid_len;

    uint8_t apdu[8] = {0, 1, 0xAA};
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(1, P1_FIRST_CHUNK, 3, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_tlv_happy_path_stores(void) {
    uint8_t tuid[32];
    size_t tuid_len = build_tuid(tuid, sizeof(tuid), 1, 0xAA);
    g_tlv_uc_tuid_ptr = tuid;
    g_tlv_uc_tuid_size = tuid_len;

    uint8_t apdu[8] = {0, 1, 0xAA};
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(1, P1_FIRST_CHUNK, 3, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 1);

    uint8_t addr[20];
    memset(addr, 0xAA, sizeof(addr));
    uint64_t chain = 1;
    const s_token_info *got = get_matching_token_info(&chain, addr);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_MEMORY(got->ticker, "USDC", 4);
}

void test_tlv_missing_tuid_tags_rejected(void) {
    // Provide a TUID buffer with only TAG_CHAIN_ID — TAG_ADDRESS missing.
    uint8_t tuid[8];
    tuid[0] = 0x23;
    tuid[1] = 1;
    tuid[2] = 1;
    g_tlv_uc_tuid_ptr = tuid;
    g_tlv_uc_tuid_size = 3;

    uint8_t apdu[8] = {0, 1, 0xAA};
    unsigned int tx = 0;
    uint16_t sw = handle_provide_erc20_token_information(1, P1_FIRST_CHUNK, 3, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mockpublic_keys_Init();
    check_signature_with_pubkey_StubWithCallback(sig_check_stub);
    reset();
}

void tearDown(void) {
    Mockpublic_keys_Verify();
    Mockpublic_keys_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unknown_p1_rejected);
    RUN_TEST(test_legacy_p2_nonzero_rejected);
    RUN_TEST(test_legacy_happy_path);
    RUN_TEST(test_legacy_truncated_missing_ticker);
    RUN_TEST(test_legacy_truncated_missing_address);
    RUN_TEST(test_legacy_truncated_missing_decimals);
    RUN_TEST(test_legacy_truncated_missing_chain_id);
    RUN_TEST(test_legacy_missing_signature_rejected);
    RUN_TEST(test_legacy_decimals_above_uint8_rejected);
    RUN_TEST(test_legacy_unsupported_chain_id_rejected);
    RUN_TEST(test_legacy_signature_failure_does_not_store);
    RUN_TEST(test_legacy_ticker_too_long_rejected);
    RUN_TEST(test_legacy_duplicate_returns_same_index);
    RUN_TEST(test_legacy_second_descriptor_gets_next_index);
    RUN_TEST(test_tlv_use_case_failure_rejects);
    RUN_TEST(test_tlv_coin_type_mismatch_rejects);
    RUN_TEST(test_tlv_happy_path_stores);
    RUN_TEST(test_tlv_missing_tuid_tags_rejected);
    return UNITY_END();
}
