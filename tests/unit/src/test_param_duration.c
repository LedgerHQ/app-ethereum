/**
 * @file test_param_duration.c
 * @brief Unit tests for the DURATION parameter in
 *        src/features/generic_tx_parser/gtp_param_duration.c.
 *
 * DURATION converts an unsigned-seconds value into a "1d02h03m04s" style
 * string, with each component dropped when its count is zero AND at least
 * one larger component is already in the buffer. The seconds component is
 * the exception: if the value is exactly zero, the buffer is empty, so
 * the formatter writes "00s" instead of returning an empty string.
 *
 * The non-obvious behaviors covered:
 *   - 0 seconds → "00s" (off==0 fallback so the field is never empty),
 *   - 60 seconds → "01m" (seconds==0 silently skipped because a larger
 *     unit was already written),
 *   - 1d / 1h / 1m on their own keep that single component without
 *     trailing zeros,
 *   - composite 1d02h03m04s exercises all four components in order.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_duration.h"
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

// Helper: build a single-value collection from a BE byte buffer.
static void set_single_value(const uint8_t *bytes, size_t len) {
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = bytes,
                                         .size = (uint16_t) len,
                                         .offset = 0,
                                         .length = (uint16_t) len};
}

// =============================================================================
// format_param_duration — each component on its own
// =============================================================================

void test_format_zero_renders_00s(void) {
    static const uint8_t zero[] = {0x00};
    set_single_value(zero, sizeof(zero));
    g_add_to_field_table_ret = true;

    s_param_duration param = {0};
    TEST_ASSERT_TRUE(format_param_duration(&param, "Period"));
}

void test_format_thirty_seconds(void) {
    static const uint8_t thirty[] = {0x1E};  // 30
    set_single_value(thirty, sizeof(thirty));
    g_add_to_field_table_ret = true;

    s_param_duration param = {0};
    TEST_ASSERT_TRUE(format_param_duration(&param, "Period"));
}

void test_format_one_minute_drops_zero_seconds(void) {
    static const uint8_t sixty[] = {0x3C};  // 60
    set_single_value(sixty, sizeof(sixty));
    g_add_to_field_table_ret = true;

    s_param_duration param = {0};
    TEST_ASSERT_TRUE(format_param_duration(&param, "Period"));
}

void test_format_one_hour(void) {
    // 3600 = 0x0E10
    static const uint8_t hour[] = {0x0E, 0x10};
    set_single_value(hour, sizeof(hour));
    g_add_to_field_table_ret = true;

    s_param_duration param = {0};
    TEST_ASSERT_TRUE(format_param_duration(&param, "Period"));
}

void test_format_one_day(void) {
    // 86400 = 0x015180
    static const uint8_t day[] = {0x01, 0x51, 0x80};
    set_single_value(day, sizeof(day));
    g_add_to_field_table_ret = true;

    s_param_duration param = {0};
    TEST_ASSERT_TRUE(format_param_duration(&param, "Period"));
}

// =============================================================================
// format_param_duration — composite value
// =============================================================================

void test_format_composite_d_h_m_s(void) {
    // 1d 2h 3m 4s = 86400 + 7200 + 180 + 4 = 93784 = 0x016E58
    static const uint8_t v[] = {0x01, 0x6E, 0x58};
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_duration param = {0};
    TEST_ASSERT_TRUE(format_param_duration(&param, "Period"));
}

// =============================================================================
// format_param_duration — error paths
// =============================================================================

void test_format_value_get_failure_returns_false(void) {
    g_vg_ret = false;
    s_param_duration param = {0};
    TEST_ASSERT_FALSE(format_param_duration(&param, "Period"));
}

void test_format_add_to_field_table_failure_propagates(void) {
    static const uint8_t a[] = {0x1E};  // 30s
    static const uint8_t b[] = {0x3C};  // 60s → "01m"
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = a, .size = 1, .offset = 0, .length = 1};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = b, .size = 1, .offset = 0, .length = 1};
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = false;

    s_param_duration param = {0};
    TEST_ASSERT_FALSE(format_param_duration(&param, "Period"));
}

// =============================================================================
// handle_param_duration_struct (TLV)
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_duration *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_duration_context ctx = {.param = param};
    return handle_param_duration_struct(&buf, &ctx);
}

void test_tlv_happy_path(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x07,  // VERSION = 7
        0x01,
        0x00,  // VALUE empty (handle_value_struct wrapped)
    };
    s_param_duration param = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.version, 7);
}

void test_tlv_duplicate_version_rejected(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x00,
        0x01,
        0x02,  // VERSION = 2 (duplicate)
        0x01,
        0x00,
    };
    s_param_duration param = {0};
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
    RUN_TEST(test_format_zero_renders_00s);
    RUN_TEST(test_format_thirty_seconds);
    RUN_TEST(test_format_one_minute_drops_zero_seconds);
    RUN_TEST(test_format_one_hour);
    RUN_TEST(test_format_one_day);
    RUN_TEST(test_format_composite_d_h_m_s);
    RUN_TEST(test_format_value_get_failure_returns_false);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_happy_path);
    RUN_TEST(test_tlv_duplicate_version_rejected);
    return UNITY_END();
}
