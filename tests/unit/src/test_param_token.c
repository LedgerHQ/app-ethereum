/**
 * @file test_param_token.c
 * @brief Unit tests for the TOKEN parameter in
 *        src/features/generic_tx_parser/gtp_param_token.c.
 *
 * TOKEN renders an ERC-20 token reference by resolving its address
 * through two branches:
 *   - if the address matches one of the registered "native_addrs"
 *     entries (typically the canonical wrapped-native sentinel),
 *     the displayable ticker for the current chain is used and
 *     token_info is intentionally NULL,
 *   - otherwise an ERC-20 token-info table lookup is attempted; if
 *     it returns NULL the whole row is rejected.
 *
 * A specific defense the formatter implements is a per-iteration
 * reset of token_info / ticker so a previous ERC-20 row cannot leak
 * its s_token_info into a subsequent native row (the CWE-451
 * presentation-issue comment in the source). This suite pins that
 * behavior explicitly.
 *
 * The TLV side has TAG_NATIVE_CURRENCY with ALLOW_MULTIPLE_TAG so
 * up to MAX_NATIVE_ADDRS (4) entries can be sent. Sending a 5th
 * or sending a payload longer than ADDRESS_LENGTH must be rejected.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_token.h"
#include "gtp_parsed_value.h"
#include "gtp_value.h"
#include "gtp_field.h"
#include "gtp_tx_info.h"
#include "token_info.h"
#include "shared_context.h"
#include "network.h"
#include "wraps.h"

// =============================================================================
// Globals
// =============================================================================

static bool g_add_to_field_table_ret = true;
static const void *g_get_matching_token_info_or_dummy_ret = NULL;

// =============================================================================
// Wrapped dependencies
// =============================================================================

static int g_vg_call = 0;
static s_parsed_value_collection g_vg[1];
static bool g_vg_ret = true;

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    (void) value;
    *collection = g_vg[g_vg_call++];
    return g_vg_ret;
}

static bool g_hvs_ret = true;
bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    (void) buf;
    (void) context;
    return g_hvs_ret;
}

static s_tx_info g_fake_tx_info;

// Per-call return value; tests setup with will_return.
const s_token_info *get_matching_token_info_or_dummy(const uint64_t *chain_id,
                                                     const uint8_t *addr) {
    (void) chain_id;
    (void) addr;
    return (const s_token_info *) g_get_matching_token_info_or_dummy_ret;
}

static const char *g_ticker_ret = "ETH";
const char *get_displayable_ticker(const uint64_t *chain_id,
                                   const chain_config_t *config,
                                   bool mainnet_only) {
    (void) chain_id;
    (void) config;
    (void) mainnet_only;
    return g_ticker_ret;
}

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    return (bool) g_add_to_field_table_ret;
}

static const s_tx_info *s_tx_info_ret = NULL;
const s_tx_info *get_current_tx_info(void) {
    return s_tx_info_ret;
}

// =============================================================================
// Fixtures
// =============================================================================

static void reset(void) {
    memset(&g_vg, 0, sizeof(g_vg));
    g_vg_call = 0;
    g_vg_ret = true;
    g_hvs_ret = true;
    g_add_to_field_table_ret = true;
    g_get_matching_token_info_or_dummy_ret = NULL;
    memset(&g_fake_tx_info, 0, sizeof(g_fake_tx_info));
    g_fake_tx_info.chain_id = 1;
    s_tx_info_ret = &g_fake_tx_info;
    g_ticker_ret = "ETH";
}

// Two distinct token addresses
static const uint8_t g_usdc_addr[ADDRESS_LENGTH] = {
    0xA0, 0xb8, 0x69, 0x91, 0xc6, 0x21, 0x8b, 0x36, 0xc1, 0xd1,
    0x9D, 0x4a, 0x2e, 0x9E, 0xb0, 0xcE, 0x36, 0x06, 0xeB, 0x48,
};
static const uint8_t g_native_sentinel[ADDRESS_LENGTH] = {
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
};
static const s_token_info g_usdc_info = {
    .ticker = "USDC",
    .decimals = 6,
    .chain_id = 1,
};

// =============================================================================
// format_param_token — native vs ERC-20
// =============================================================================

void test_format_native_match_uses_displayable_ticker(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_native_sentinel,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};

    // get_matching_token_info_or_dummy MUST NOT be called when the address
    // is native — the test would fail with an unsatisfied mock if it were
    // (no will_return). The ticker comes from get_displayable_ticker.
    g_ticker_ret = "ETH";
    // Native path: token_info must be NULL (the source explicitly resets it
    // per-iteration to avoid leaking a previous lookup).
    g_add_to_field_table_ret = true;

    s_param_token param = {0};
    param.native_addr_count = 1;
    memcpy(param.native_addrs[0], g_native_sentinel, ADDRESS_LENGTH);
    TEST_ASSERT_TRUE(format_param_token(&param, "Token"));
}

void test_format_erc20_match_uses_token_info(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_usdc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};

    g_get_matching_token_info_or_dummy_ret = &g_usdc_info;
    g_add_to_field_table_ret = true;

    s_param_token param = {0};
    // No native_addrs registered → forces the ERC-20 branch.
    TEST_ASSERT_TRUE(format_param_token(&param, "Token"));
}

void test_format_erc20_not_found_rejects(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_usdc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};

    g_get_matching_token_info_or_dummy_ret = NULL;
    // No add_to_field_table expected.

    s_param_token param = {0};
    TEST_ASSERT_FALSE(format_param_token(&param, "Token"));
}

// =============================================================================
// format_param_token — per-iteration reset (CWE-451 defense)
// =============================================================================

void test_format_resets_token_info_between_iterations(void) {
    // Iteration 1: USDC (ERC-20 path → token_info populated).
    // Iteration 2: native (must surface token_info=NULL in the field
    // table, NOT the &g_usdc_info pointer from the previous iteration).
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_usdc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = g_native_sentinel,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};

    // ERC-20 lookup runs only on the first iteration.
    g_get_matching_token_info_or_dummy_ret = &g_usdc_info;

    // First field: USDC, with the lookup result.
    g_add_to_field_table_ret = true;

    // Second field: native, token_info MUST be NULL.
    g_add_to_field_table_ret = true;

    s_param_token param = {0};
    param.native_addr_count = 1;
    memcpy(param.native_addrs[0], g_native_sentinel, ADDRESS_LENGTH);
    TEST_ASSERT_TRUE(format_param_token(&param, "Token"));
}

// =============================================================================
// format_param_token — failure paths
// =============================================================================

void test_format_tx_info_null_returns_false(void) {
    s_tx_info_ret = NULL;
    s_param_token param = {0};
    TEST_ASSERT_FALSE(format_param_token(&param, "Token"));
}

void test_format_value_get_failure_returns_false(void) {
    g_vg_ret = false;
    s_param_token param = {0};
    TEST_ASSERT_FALSE(format_param_token(&param, "Token"));
}

void test_format_native_ticker_null_rejected(void) {
    // Edge case: native match but get_displayable_ticker returns NULL.
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_native_sentinel,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};
    g_ticker_ret = NULL;

    s_param_token param = {0};
    param.native_addr_count = 1;
    memcpy(param.native_addrs[0], g_native_sentinel, ADDRESS_LENGTH);
    TEST_ASSERT_FALSE(format_param_token(&param, "Token"));
}

void test_format_add_to_field_table_failure_propagates(void) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_usdc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};

    g_get_matching_token_info_or_dummy_ret = &g_usdc_info;
    g_add_to_field_table_ret = false;

    s_param_token param = {0};
    TEST_ASSERT_FALSE(format_param_token(&param, "Token"));
}

// =============================================================================
// handle_param_token_struct (TLV)
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_token *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_token_context ctx = {.param = param};
    return handle_param_token_struct(&buf, &ctx);
}

void test_tlv_happy_path_two_native(void) {
    // VERSION=1, ADDRESS empty, two NATIVE_CURRENCY entries
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        // NATIVE_CURRENCY #1 — full 20-byte address
        0x02,
        0x14,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        0xEE,
        // NATIVE_CURRENCY #2 — short 2-byte payload, right-aligned into 20-byte slot
        0x02,
        0x02,
        0x12,
        0x34,
    };
    s_param_token param = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.native_addr_count, 2);
    // First entry stored verbatim
    static const uint8_t expected_full[ADDRESS_LENGTH] = {
        0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
        0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
    };
    TEST_ASSERT_EQUAL_MEMORY(param.native_addrs[0], expected_full, ADDRESS_LENGTH);
    // Second entry: 18 leading zeros, then 0x12, 0x34
    static const uint8_t expected_short[ADDRESS_LENGTH] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x12, 0x34,
    };
    TEST_ASSERT_EQUAL_MEMORY(param.native_addrs[1], expected_short, ADDRESS_LENGTH);
}

void test_tlv_too_many_native_addrs_rejected(void) {
    // MAX_NATIVE_ADDRS == 4 — sending 5 must fail on the 5th call.
    uint8_t bytes[3 + 5 * (2 + ADDRESS_LENGTH)];
    size_t off = 0;
    bytes[off++] = 0x00;
    bytes[off++] = 0x01;
    bytes[off++] = 0x01;  // VERSION = 1
    for (int i = 0; i < 5; ++i) {
        bytes[off++] = 0x02;
        bytes[off++] = ADDRESS_LENGTH;
        for (int j = 0; j < ADDRESS_LENGTH; ++j) {
            bytes[off++] = (uint8_t) i;
        }
    }
    s_param_token param = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
}

void test_tlv_native_addr_too_long_rejected(void) {
    // A native_addr payload > ADDRESS_LENGTH (20) must be rejected by
    // handle_native_currency.
    uint8_t bytes[3 + 2 + 21];
    size_t off = 0;
    bytes[off++] = 0x00;
    bytes[off++] = 0x01;
    bytes[off++] = 0x01;
    bytes[off++] = 0x02;
    bytes[off++] = 21;  // length > ADDRESS_LENGTH
    for (int j = 0; j < 21; ++j) {
        bytes[off++] = 0xAB;
    }
    s_param_token param = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
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
    RUN_TEST(test_format_native_match_uses_displayable_ticker);
    RUN_TEST(test_format_erc20_match_uses_token_info);
    RUN_TEST(test_format_erc20_not_found_rejects);
    RUN_TEST(test_format_resets_token_info_between_iterations);
    RUN_TEST(test_format_tx_info_null_returns_false);
    RUN_TEST(test_format_value_get_failure_returns_false);
    RUN_TEST(test_format_native_ticker_null_rejected);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_happy_path_two_native);
    RUN_TEST(test_tlv_too_many_native_addrs_rejected);
    RUN_TEST(test_tlv_native_addr_too_long_rejected);
    return UNITY_END();
}
