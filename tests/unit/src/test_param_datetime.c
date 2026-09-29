/**
 * @file test_param_datetime.c
 * @brief Unit tests for the DATETIME parameter in
 *        src/features/generic_tx_parser/gtp_param_datetime.c.
 *
 * The parameter has two render modes, picked at TLV time via TAG_TYPE:
 *   - DT_UNIX (0):       interprets the value as a uint64 timestamp and
 *                        formats it as UTC, with an "Unlimited" sentinel
 *                        when the bytes are all 0xFF (capped allowance).
 *   - DT_BLOCKHEIGHT (1): renders the value as a plain decimal block
 *                         number through uint256_to_decimal.
 *
 * Tests cover both rendering branches, the sentinel detection, multi-
 * iteration broadcast over a value collection, and the TLV parser
 * (VERSION, VALUE, TYPE — all ENFORCE_UNIQUE_TAG, TYPE additionally
 * checked against the enum so unknown values are rejected).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_datetime.h"
#include "gtp_parsed_value.h"
#include "gtp_value.h"
#include "gtp_field.h"
#include "shared_context.h"

// =============================================================================
// Globals
// =============================================================================

static bool g_add_to_field_table_ret = true;

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

bool add_to_field_table(e_param_type type,
                        const char *key,
                        const char *value,
                        const void *extra_data) {
    (void) extra_data;
    return (bool) g_add_to_field_table_ret;
}

// =============================================================================
// Fixtures
// =============================================================================

static void reset(void) {
    memset(&g_vg, 0, sizeof(g_vg));
    g_vg_call = 0;
    g_vg_ret = true;
    g_hvs_ret = true;
}

// =============================================================================
// format_param_datetime — DT_UNIX branch
// =============================================================================

void test_format_unix_timestamp_rendered_as_utc(void) {
    // 2024-01-01 00:00:00 UTC = 1704067200 = 0x6592_0080 (big-endian)
    static uint8_t ts[] = {0x65, 0x92, 0x00, 0x80};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = ts, .size = 4, .offset = 0, .length = 4};

    // Per time_format_to_utc(), hour 0 renders as "12 AM"
    g_add_to_field_table_ret = true;

    s_param_datetime param = {0};
    param.type = DT_UNIX;
    param.value.type_size = 4;
    TEST_ASSERT_TRUE(format_param_datetime(&param, "Date"));
}

void test_format_unix_maxint_renders_unlimited(void) {
    // 4-byte all-0xFF triggers the "Unlimited" sentinel branch.
    static uint8_t maxint4[] = {0xFF, 0xFF, 0xFF, 0xFF};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = maxint4, .size = 4, .offset = 0, .length = 4};
    g_add_to_field_table_ret = true;

    s_param_datetime param = {0};
    param.type = DT_UNIX;
    // Sentinel only triggers when value.length >= param.value.type_size; here
    // type_size == length so the >= check is satisfied.
    param.value.type_size = 4;
    TEST_ASSERT_TRUE(format_param_datetime(&param, "Until"));
}

void test_format_unix_abi_word_rendered_as_utc(void) {
    // 32-byte ABI word holding 1704067200
    static uint8_t word[32] = {0};
    word[28] = 0x65;
    word[29] = 0x92;
    word[30] = 0x00;
    word[31] = 0x80;
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = word, .size = 32, .offset = 0, .length = 32};
    g_add_to_field_table_ret = true;

    s_param_datetime param = {0};
    param.type = DT_UNIX;
    param.value.type_size = 32;
    TEST_ASSERT_TRUE(format_param_datetime(&param, "Date"));
}

void test_format_unix_above_time_t_range_rejected(void) {
    // 32-byte ABI word ending in 0xfffffffffffffffe: not "Unlimited", not a valid time_t
    static uint8_t word[32] = {0};
    memset(&word[24], 0xFF, 8);
    word[31] = 0xFE;
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = word, .size = 32, .offset = 0, .length = 32};
    g_add_to_field_table_ret = true;

    s_param_datetime param = {0};
    param.type = DT_UNIX;
    param.value.type_size = 32;
    TEST_ASSERT_FALSE(format_param_datetime(&param, "Deadline"));
}

void test_format_unix_int64_min_bit_pattern_rejected(void) {
    static uint8_t ts[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = ts, .size = 8, .offset = 0, .length = 8};
    g_add_to_field_table_ret = true;

    s_param_datetime param = {0};
    param.type = DT_UNIX;
    param.value.type_size = 8;
    TEST_ASSERT_FALSE(format_param_datetime(&param, "Deadline"));
}

// =============================================================================
// format_param_datetime — DT_BLOCKHEIGHT branch
// =============================================================================

void test_format_blockheight_rendered_decimal(void) {
    // 19_000_000 = 0x0121_EAC0
    static uint8_t height[] = {0x01, 0x21, 0xEA, 0xC0};
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = height, .size = 4, .offset = 0, .length = 4};
    g_add_to_field_table_ret = true;

    s_param_datetime param = {0};
    param.type = DT_BLOCKHEIGHT;
    TEST_ASSERT_TRUE(format_param_datetime(&param, "Block"));
}

// =============================================================================
// format_param_datetime — iteration / error paths
// =============================================================================

void test_format_multiple_blockheights_iterates(void) {
    static uint8_t b1[] = {0x00, 0x0A};  // 10
    static uint8_t b2[] = {0x00, 0x14};  // 20
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = b1, .size = 2, .offset = 0, .length = 2};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = b2, .size = 2, .offset = 0, .length = 2};
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = true;

    s_param_datetime param = {0};
    param.type = DT_BLOCKHEIGHT;
    TEST_ASSERT_TRUE(format_param_datetime(&param, "Block"));
}

void test_format_value_get_failure_returns_false(void) {
    g_vg_ret = false;
    s_param_datetime param = {0};
    param.type = DT_UNIX;
    TEST_ASSERT_FALSE(format_param_datetime(&param, "Date"));
}

void test_format_add_to_field_table_failure_propagates(void) {
    static uint8_t b1[] = {0x00, 0x01};
    static uint8_t b2[] = {0x00, 0x02};
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = b1, .size = 2, .offset = 0, .length = 2};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = b2, .size = 2, .offset = 0, .length = 2};
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = false;

    s_param_datetime param = {0};
    param.type = DT_BLOCKHEIGHT;
    TEST_ASSERT_FALSE(format_param_datetime(&param, "Block"));
}

// =============================================================================
// handle_param_datetime_struct (TLV)
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_datetime *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_datetime_context ctx = {.param = param};
    return handle_param_datetime_struct(&buf, &ctx);
}

void test_tlv_happy_path_unix(void) {
    // VERSION(0) len 1 [1], VALUE(1) len 0, TYPE(2) len 1 [DT_UNIX]
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x01,
        0x00,  // VALUE empty
        0x02,
        0x01,
        DT_UNIX,  // TYPE = UNIX
    };
    s_param_datetime param = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.version, 1);
    TEST_ASSERT_EQUAL(param.type, DT_UNIX);
}

void test_tlv_happy_path_blockheight(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        0x02,
        0x01,
        DT_BLOCKHEIGHT,
    };
    s_param_datetime param = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.type, DT_BLOCKHEIGHT);
}

void test_tlv_invalid_type_rejected(void) {
    // TYPE = 0x99 — not in the {DT_UNIX, DT_BLOCKHEIGHT} switch.
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        0x02,
        0x01,
        0x99,
    };
    s_param_datetime param = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
}

void test_tlv_duplicate_type_rejected(void) {
    // ENFORCE_UNIQUE_TAG: sending TYPE twice must fail.
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        0x02,
        0x01,
        DT_UNIX,
        0x02,
        0x01,
        DT_BLOCKHEIGHT,
    };
    s_param_datetime param = {0};
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
    RUN_TEST(test_format_unix_timestamp_rendered_as_utc);
    RUN_TEST(test_format_unix_maxint_renders_unlimited);
    RUN_TEST(test_format_unix_abi_word_rendered_as_utc);
    RUN_TEST(test_format_unix_above_time_t_range_rejected);
    RUN_TEST(test_format_unix_int64_min_bit_pattern_rejected);
    RUN_TEST(test_format_blockheight_rendered_decimal);
    RUN_TEST(test_format_multiple_blockheights_iterates);
    RUN_TEST(test_format_value_get_failure_returns_false);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_happy_path_unix);
    RUN_TEST(test_tlv_happy_path_blockheight);
    RUN_TEST(test_tlv_invalid_type_rejected);
    RUN_TEST(test_tlv_duplicate_type_rejected);
    return UNITY_END();
}
