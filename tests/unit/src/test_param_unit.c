/**
 * @file test_param_unit.c
 * @brief Unit tests for the UNIT parameter in
 *        src/features/gcs/gcs_param_unit.c.
 *
 * UNIT renders a numeric value with a unit suffix, e.g. "1000 USDC" or
 * "10.00 USDC", after rescaling the input by param->decimals. The
 * formatter has three guards beyond the usual value-collection iteration:
 *   - uint256_to_decimal must succeed (caps at INT256_LENGTH input),
 *   - adjustDecimals must succeed (caps at the destination buffer size),
 *   - param->base must be non-empty — an empty unit suffix is treated
 *     as a malformed parameter rather than rendered with no suffix.
 *
 * The TLV side has an additional handler per tag compared to amount /
 * duration: TAG_BASE copies a NUL-terminated string (rejecting payloads
 * >= 11 bytes so the trailing NUL always fits) and TAG_PREFIX is a bool
 * that has to be false: the prefix is not applied to the value, so a
 * descriptor asking for it is refused.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gcs_param_unit.h"
#include "gcs_parsed_value.h"
#include "gcs_value.h"
#include "gcs_field.h"
#include "shared_context.h"

// =============================================================================
// Globals
// =============================================================================

static bool g_add_to_field_table_ret = true;
static int g_root_value_shown_calls = 0;

void value_set_shown(const s_value *value, uint8_t index) {
    (void) value;
    (void) index;
    g_root_value_shown_calls++;
}
static char g_last_value[80];

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
    strlcpy(g_last_value, value, sizeof(g_last_value));
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
    g_last_value[0] = '\0';
    g_root_value_shown_calls = 0;
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
// format_param_unit — happy paths
// =============================================================================

void test_format_no_decimals(void) {
    // 1000 decimal = 0x03E8
    static const uint8_t v[] = {0x03, 0xE8};
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    strlcpy(param.base, "USDC", sizeof(param.base));
    param.decimals = 0;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
    TEST_ASSERT_EQUAL_INT(1, g_root_value_shown_calls);
}

void test_format_with_decimals(void) {
    // 1000 with decimals=2 → "10" (trailing fractional zeros are trimmed)
    static const uint8_t v[] = {0x03, 0xE8};
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    strlcpy(param.base, "USDC", sizeof(param.base));
    param.decimals = 2;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
}

void test_format_fractional_with_decimals(void) {
    // 1234 with decimals=2 → "12.34"
    static const uint8_t v[] = {0x04, 0xD2};
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    strlcpy(param.base, "USDC", sizeof(param.base));
    param.decimals = 2;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
}

void test_format_iterates_multiple_values(void) {
    static const uint8_t a[] = {0x01};
    static const uint8_t b[] = {0x02};
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = a, .size = 1, .offset = 0, .length = 1};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = b, .size = 1, .offset = 0, .length = 1};
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    strlcpy(param.base, "PT", sizeof(param.base));
    param.decimals = 0;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
}

void test_format_signed_word_negative(void) {
    // int64 -1234 as a sign-extended 32-byte ABI word, decimals=2 → "-12.34"
    static uint8_t v[32];
    memset(v, 0xFF, sizeof(v));
    v[30] = 0xFB;
    v[31] = 0x2E;
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    param.value.type_family = TF_INT;
    param.value.type_size = 8;
    strlcpy(param.base, "USDC", sizeof(param.base));
    param.decimals = 2;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
    TEST_ASSERT_EQUAL_STRING("-12.34 USDC", g_last_value);
}

void test_format_signed_negative_below_one(void) {
    // int8 -1 with decimals=2 → "-0.01"
    static const uint8_t v[] = {0xFF};
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    param.value.type_family = TF_INT;
    param.value.type_size = 1;
    strlcpy(param.base, "PT", sizeof(param.base));
    param.decimals = 2;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
    TEST_ASSERT_EQUAL_STRING("-0.01 PT", g_last_value);
}

void test_format_signed_positive(void) {
    static const uint8_t v[] = {0x00, 0x00, 0x03, 0xE8};
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    param.value.type_family = TF_INT;
    param.value.type_size = 4;
    strlcpy(param.base, "PT", sizeof(param.base));
    param.decimals = 0;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
    TEST_ASSERT_EQUAL_STRING("1000 PT", g_last_value);
}

void test_format_unsigned_high_bit_stays_positive(void) {
    static const uint8_t v[] = {0xFF};
    set_single_value(v, sizeof(v));
    g_add_to_field_table_ret = true;

    s_param_unit param = {0};
    param.value.type_family = TF_UINT;
    param.value.type_size = 1;
    strlcpy(param.base, "PT", sizeof(param.base));
    param.decimals = 0;
    TEST_ASSERT_TRUE(format_param_unit(&param, "Stake"));
    TEST_ASSERT_EQUAL_STRING("255 PT", g_last_value);
}

// =============================================================================
// format_param_unit — failure paths
// =============================================================================

void test_format_value_get_failure_returns_false(void) {
    g_vg_ret = false;
    s_param_unit param = {0};
    strlcpy(param.base, "USDC", sizeof(param.base));
    TEST_ASSERT_FALSE(format_param_unit(&param, "Stake"));
}

void test_format_empty_base_rejected(void) {
    static const uint8_t v[] = {0x01};
    set_single_value(v, sizeof(v));
    // base[0] == '\0' triggers the explicit reject branch BEFORE
    // add_to_field_table — no add_* expectations.

    s_param_unit param = {0};
    // param.base stays all-NUL from the {0} initializer
    TEST_ASSERT_FALSE(format_param_unit(&param, "Stake"));
}

void test_format_add_to_field_table_failure_propagates(void) {
    static const uint8_t a[] = {0x01};
    static const uint8_t b[] = {0x02};
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = a, .size = 1, .offset = 0, .length = 1};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = b, .size = 1, .offset = 0, .length = 1};
    g_add_to_field_table_ret = true;
    g_add_to_field_table_ret = false;

    s_param_unit param = {0};
    strlcpy(param.base, "PT", sizeof(param.base));
    param.decimals = 0;
    TEST_ASSERT_FALSE(format_param_unit(&param, "Stake"));
}

// =============================================================================
// handle_param_unit_struct (TLV)
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_param_unit *param) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_param_unit_context ctx = {.param = param};
    return handle_param_unit_struct(&buf, &ctx);
}

void test_tlv_version_other_than_one_refused(void) {
    uint8_t bytes[] =
        {0x00, 0x01, 0x01, 0x01, 0x00, 0x02, 0x04, 'U', 'S', 'D', 'C', 0x03, 0x01, 0x06};
    const uint8_t other_versions[] = {0x00, 0x02, 0xFF};
    s_param_unit param = {0};

    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    for (size_t i = 0; i < sizeof(other_versions); i++) {
        bytes[2] = other_versions[i];
        TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
    }
}

void test_tlv_happy_path(void) {
    // VERSION=1, VALUE empty, BASE="USDC", DECIMALS=6, PREFIX=false
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        0x02,
        0x04,
        'U',
        'S',
        'D',
        'C',
        0x03,
        0x01,
        0x06,
        0x04,
        0x01,
        0x00,
    };
    s_param_unit param = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &param));
    TEST_ASSERT_EQUAL(param.version, 1);
    TEST_ASSERT_EQUAL_STRING(param.base, "USDC");
    TEST_ASSERT_EQUAL(param.decimals, 6);
}

void test_tlv_base_too_long_rejected(void) {
    // BASE_STR_SIZE == 11 → handle_base rejects payloads >= 11 (need
    // room for the trailing NUL). Send an 11-byte payload.
    const uint8_t bytes[] = {
        0x00, 0x01, 0x01, 0x01, 0x00, 0x02, 0x0B, 'A',  'B',  'C',  'D',
        'E',  'F',  'G',  'H',  'I',  'J',  'K',  0x03, 0x01, 0x00,
    };
    s_param_unit param = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &param));
}

// PREFIX is a bool. The prefix is not applied to the value, so a descriptor that asks for it is
// refused, and one that does not is accepted
static bool run_with_prefix(const uint8_t *prefix, size_t prefix_size) {
    uint8_t bytes[32] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        0x02,
        0x02,
        'B',
        'B',
        0x03,
        0x01,
        0x00,
        0x04,
    };
    size_t size = 13;
    s_param_unit param = {0};

    bytes[size++] = (uint8_t) prefix_size;
    if (prefix_size > 0) {
        memcpy(&bytes[size], prefix, prefix_size);
    }
    size += prefix_size;
    return run_tlv(bytes, size, &param);
}

void test_tlv_prefix_false_accepted(void) {
    const uint8_t no_prefix[] = {0x00};
    TEST_ASSERT_TRUE(run_with_prefix(no_prefix, sizeof(no_prefix)));
}

void test_tlv_prefix_true_refused(void) {
    const uint8_t prefix[] = {0x01};
    TEST_ASSERT_FALSE(run_with_prefix(prefix, sizeof(prefix)));
}

void test_tlv_prefix_that_is_not_a_bool_refused(void) {
    const uint8_t two[] = {0x02};
    const uint8_t max[] = {0xFF};
    TEST_ASSERT_FALSE(run_with_prefix(two, sizeof(two)));
    TEST_ASSERT_FALSE(run_with_prefix(max, sizeof(max)));
}

void test_tlv_prefix_empty_refused(void) {
    TEST_ASSERT_FALSE(run_with_prefix(NULL, 0));
}

void test_tlv_duplicate_base_rejected(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,
        0x01,
        0x00,
        0x02,
        0x02,
        'A',
        'B',
        0x02,
        0x02,
        'C',
        'D',  // duplicate BASE
        0x03,
        0x01,
        0x00,
    };
    s_param_unit param = {0};
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
    RUN_TEST(test_format_no_decimals);
    RUN_TEST(test_format_with_decimals);
    RUN_TEST(test_format_fractional_with_decimals);
    RUN_TEST(test_format_iterates_multiple_values);
    RUN_TEST(test_format_signed_word_negative);
    RUN_TEST(test_format_signed_negative_below_one);
    RUN_TEST(test_format_signed_positive);
    RUN_TEST(test_format_unsigned_high_bit_stays_positive);
    RUN_TEST(test_format_value_get_failure_returns_false);
    RUN_TEST(test_format_empty_base_rejected);
    RUN_TEST(test_format_add_to_field_table_failure_propagates);
    RUN_TEST(test_tlv_version_other_than_one_refused);
    RUN_TEST(test_tlv_happy_path);
    RUN_TEST(test_tlv_base_too_long_rejected);
    RUN_TEST(test_tlv_prefix_false_accepted);
    RUN_TEST(test_tlv_prefix_true_refused);
    RUN_TEST(test_tlv_prefix_that_is_not_a_bool_refused);
    RUN_TEST(test_tlv_prefix_empty_refused);
    RUN_TEST(test_tlv_duplicate_base_rejected);
    return UNITY_END();
}
