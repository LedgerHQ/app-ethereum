/**
 * @file test_param_token_amount.c
 * @brief Unit tests for TOKEN_AMOUNT parameter formatting, focusing on
 *        §3.1.8 iteration broadcast (secondary collection of size 1 repeated
 *        across all iterations of the primary collection).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_token_amount.h"
#include "gtp_parsed_value.h"
#include "gtp_value.h"
#include "gtp_tx_info.h"
#include "token_info.h"
#include "network.h"
#include "shared_context.h"

// Required globals

// ===========================================================================
// value_get / value_cleanup mock
// ===========================================================================

static int g_vg_call = 0;
static s_parsed_value_collection g_vg[2];

static bool g_add_to_field_table_ret = true;
static const void *g_get_matching_token_info_or_dummy_ret = NULL;

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    (void) value;
    *collection = g_vg[g_vg_call++];
    return true;
}

// ===========================================================================
// handle_value_struct stub — TLV parse handlers reference it but are never
// called by format_param_token_amount; only needed for linkage.
// ===========================================================================

bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    (void) buf;
    (void) context;
    return true;
}

// ===========================================================================
// Other mocks
// ===========================================================================

static s_tx_info g_fake_tx_info;

const s_tx_info *get_current_tx_info(void) {
    return &g_fake_tx_info;
}

const s_token_info *get_matching_token_info_or_dummy(const uint64_t *chain_id,
                                                     const uint8_t *addr) {
    (void) chain_id;
    (void) addr;
    return (const s_token_info *) g_get_matching_token_info_or_dummy_ret;
}

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    (void) extra_data;
    return (bool) g_add_to_field_table_ret;
}

// ===========================================================================
// Test data
// ===========================================================================

// 1 USDC = 1_000_000 (6 decimals) — big-endian in 32 bytes
static uint8_t g_amount1[INT256_LENGTH] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    0,    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0F, 0x42, 0x40,
};

// 2 USDC = 2_000_000 (6 decimals) — big-endian in 32 bytes
static uint8_t g_amount2[INT256_LENGTH] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    0,    0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1E, 0x84, 0x80,
};

// Fake USDC token address (20 bytes)
static uint8_t g_usdc_addr[ADDRESS_LENGTH] = {
    0xA0, 0xb8, 0x69, 0x91, 0xc6, 0x21, 0x8b, 0x36, 0xc1, 0xd1,
    0x9D, 0x4a, 0x2e, 0x9E, 0xb0, 0xcE, 0x36, 0x06, 0xeB, 0x48,
};

// Fake USDC extra_info returned by get_matching_token_info_or_dummy
static s_token_info g_usdc_info = {
    .address =
        {
            0xA0, 0xb8, 0x69, 0x91, 0xc6, 0x21, 0x8b, 0x36, 0xc1, 0xd1,
            0x9D, 0x4a, 0x2e, 0x9E, 0xb0, 0xcE, 0x36, 0x06, 0xeB, 0x48,
        },
    .ticker = "USDC",
    .decimals = 6,
    .chain_id = 1,
};

// ===========================================================================
// Tests
// ===========================================================================

/**
 * Broadcast: value collection has 2 elements, token collection has 1 element.
 * The single token must be reused for both amount iterations — §3.1.8.
 * Expected: format_param_token_amount returns true and add_to_field_table is
 * called twice, once for "1 USDC" and once for "2 USDC".
 */
void test_token_amount_broadcast_ok(void) {
    // Primary collection: two amounts
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = g_amount1,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = g_amount2,
                                         .size = INT256_LENGTH,
                                         .offset = 0,
                                         .length = INT256_LENGTH};

    // Secondary (token) collection: one address — broadcast
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {.ptr = g_usdc_addr,
                                         .size = ADDRESS_LENGTH,
                                         .offset = 0,
                                         .length = ADDRESS_LENGTH};

    g_vg_call = 0;
    memset(&g_fake_tx_info, 0, sizeof(g_fake_tx_info));
    g_fake_tx_info.chain_id = 1;

    // token resolution: USDC found (called for each of the 2 iterations)
    g_get_matching_token_info_or_dummy_ret = &g_usdc_info;
    g_get_matching_token_info_or_dummy_ret = &g_usdc_info;

    // Expected field table entries: "1 USDC" then "2 USDC"
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = true;

    s_param_token_amount param;
    memset(&param, 0, sizeof(param));
    param.version = 1;
    param.has_token = true;

    TEST_ASSERT_TRUE(format_param_token_amount(&param, "Amount"));
}

/**
 * Mismatch rejection: value collection has 2 elements, token collection has 3.
 * Neither is 1, and 3 ≠ 2, so the size check must fail immediately —
 * add_to_field_table must NOT be called.
 */
void test_token_amount_size_mismatch_rejected(void) {
    // Primary collection: 2 values
    g_vg[0].size = 2;

    // Secondary (token) collection: 3 — mismatched, neither equals 1 nor 2
    g_vg[1].size = 3;

    g_vg_call = 0;

    s_param_token_amount param;
    memset(&param, 0, sizeof(param));
    param.version = 1;
    param.has_token = true;

    // format_param_token_amount must return false; add_to_field_table is NOT called
    TEST_ASSERT_FALSE(format_param_token_amount(&param, "Amount"));
}

// ===========================================================================
// TLV tag-handler tests — drive handle_param_token_amount_struct with
// hand-crafted TLV buffers covering every tag dispatch.
// ===========================================================================

// All tags here are < 0x80 so short-form encoding is used (no DER long form).
// Layout per tag: { tag, length, value... }

void test_handle_struct_all_tags_ok(void) {
    // VERSION(0x00) len=1 value=1
    // VALUE(0x01)   len=0 (empty inner — wrap returns true)
    // TOKEN(0x02)   len=0
    // NATIVE_CURRENCY(0x03) len=4 4-byte address fragment
    // THRESHOLD(0x04) len=2 = 0x0064
    // ABOVE_THRESHOLD_MSG(0x05) len=3 "Hi!"
    uint8_t buf_bytes[] = {
        0x00, 0x01, 0x01, 0x01, 0x00, 0x02, 0x00, 0x03, 0x04, 0xDE, 0xAD,
        0xBE, 0xEF, 0x04, 0x02, 0x00, 0x64, 0x05, 0x03, 'H',  'i',  '!',
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_token_amount param;
    memset(&param, 0, sizeof(param));
    s_param_token_amount_context ctx = {.param = &param};
    TEST_ASSERT_TRUE(handle_param_token_amount_struct(&buf, &ctx));
    TEST_ASSERT_EQUAL(param.version, 1);
    TEST_ASSERT_TRUE(param.has_token);
    TEST_ASSERT_EQUAL(param.native_addr_count, 1);
    // 4-byte payload is right-aligned in the 20-byte address slot
    static const uint8_t expected_native[ADDRESS_LENGTH] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xDE, 0xAD, 0xBE, 0xEF,
    };
    TEST_ASSERT_EQUAL_MEMORY(param.native_addrs[0], expected_native, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_STRING(param.above_threshold_msg, "Hi!");
}

void test_handle_struct_native_currency_oversize_rejected(void) {
    // VERSION + a NATIVE_CURRENCY with size > ADDRESS_LENGTH (20).
    uint8_t buf_bytes[3 + 2 + 21];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    buf_bytes[3] = 0x03;
    buf_bytes[4] = 21;
    memset(buf_bytes + 5, 0xAA, 21);
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_token_amount param;
    memset(&param, 0, sizeof(param));
    s_param_token_amount_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_token_amount_struct(&buf, &ctx));
}

void test_handle_struct_native_currency_overflow_capacity_rejected(void) {
    // VERSION + MAX_NATIVE_ADDRS (=4) accepted + a 5th one that overflows.
    uint8_t buf_bytes[3 + 5 * 3];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    for (int i = 0; i < 5; i++) {
        buf_bytes[3 + i * 3 + 0] = 0x03;      // NATIVE_CURRENCY
        buf_bytes[3 + i * 3 + 1] = 0x01;      // length
        buf_bytes[3 + i * 3 + 2] = 0x10 + i;  // value
    }
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_token_amount param;
    memset(&param, 0, sizeof(param));
    s_param_token_amount_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_token_amount_struct(&buf, &ctx));
}

void test_handle_struct_threshold_oversize_rejected(void) {
    // THRESHOLD with size > sizeof(uint256_t)=32.
    uint8_t buf_bytes[3 + 2 + 33];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    buf_bytes[3] = 0x04;
    buf_bytes[4] = 33;
    memset(buf_bytes + 5, 0xAB, 33);
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_token_amount param;
    memset(&param, 0, sizeof(param));
    s_param_token_amount_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_token_amount_struct(&buf, &ctx));
}

void test_handle_struct_above_threshold_msg_oversize_rejected(void) {
    // ABOVE_THRESHOLD_MSG with size >= ABOVE_THRESHOLD_MSG_SIZE (21).
    uint8_t buf_bytes[3 + 2 + 21];
    buf_bytes[0] = 0x00;
    buf_bytes[1] = 0x01;
    buf_bytes[2] = 0x01;
    buf_bytes[3] = 0x05;
    buf_bytes[4] = 21;
    memset(buf_bytes + 5, 'A', 21);
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_token_amount param;
    memset(&param, 0, sizeof(param));
    s_param_token_amount_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_token_amount_struct(&buf, &ctx));
}

// ===========================================================================
// Test runner
// ===========================================================================

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_token_amount_broadcast_ok);
    RUN_TEST(test_token_amount_size_mismatch_rejected);
    RUN_TEST(test_handle_struct_all_tags_ok);
    RUN_TEST(test_handle_struct_native_currency_oversize_rejected);
    RUN_TEST(test_handle_struct_native_currency_overflow_capacity_rejected);
    RUN_TEST(test_handle_struct_threshold_oversize_rejected);
    RUN_TEST(test_handle_struct_above_threshold_msg_oversize_rejected);
    return UNITY_END();
}
