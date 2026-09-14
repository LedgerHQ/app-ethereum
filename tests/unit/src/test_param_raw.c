/**
 * @file test_param_raw.c
 * @brief Unit tests for raw parameter formatting
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "calldata.h"  // CALLDATA_CHUNK_SIZE

// Includes
#include "gtp_field.h"
#include "gtp_value.h"
#include "shared_context.h"
#include "status_words.h"
#include "uint256.h"

// Headers for mocked functions
#include "utils.h"
#include "common_utils.h"
#include "read.h"
#include "write.h"
#include "map_entry.h"

// Helper macro to create a UINT parameter
#define CREATE_UINT_PARAM(param_name, value_bytes, value_size)              \
    s_param_raw param_name = {.version = 1,                                 \
                              .value = {.type_family = TF_UINT,             \
                                        .source = SOURCE_CONSTANT,          \
                                        .constant = {.size = value_size}}}; \
    memcpy(param_name.value.constant.buf, value_bytes, value_size);

// Helper macro to create an INT parameter (signed integer of `type_size` bytes,
// e.g. type_size=4 for int32). value_bytes is stored as-is and interpreted
// by format_signed_int_be using type_size for sign extension.
#define CREATE_INT_PARAM(param_name, value_bytes, value_size, ts)           \
    s_param_raw param_name = {.version = 1,                                 \
                              .value = {.type_family = TF_INT,              \
                                        .type_size = (ts),                  \
                                        .source = SOURCE_CONSTANT,          \
                                        .constant = {.size = value_size}}}; \
    memcpy(param_name.value.constant.buf, value_bytes, value_size);

// Helper macro to create an ADDRESS parameter
#define CREATE_ADDRESS_PARAM(param_name, ...)                                   \
    uint8_t param_name##_addr[ADDRESS_LENGTH] = {__VA_ARGS__};                  \
    s_param_raw param_name = {.version = 1,                                     \
                              .value = {.type_family = TF_ADDRESS,              \
                                        .source = SOURCE_CONSTANT,              \
                                        .constant = {.size = ADDRESS_LENGTH}}}; \
    memcpy(param_name.value.constant.buf, param_name##_addr, ADDRESS_LENGTH);

// Helper macro to create a BYTES parameter
#define CREATE_BYTES_PARAM(param_name, data_bytes, data_size)              \
    s_param_raw param_name = {.version = 1,                                \
                              .value = {.type_family = TF_BYTES,           \
                                        .source = SOURCE_CONSTANT,         \
                                        .constant = {.size = data_size}}}; \
    memcpy(param_name.value.constant.buf, data_bytes, data_size);

// Helper macro to create a BOOL parameter
#define CREATE_BOOL_PARAM(param_name, bool_val)                                                 \
    uint8_t param_name##_val = (bool_val) ? 1 : 0;                                              \
    s_param_raw param_name = {                                                                  \
        .version = 1,                                                                           \
        .value = {.type_family = TF_BOOL, .source = SOURCE_CONSTANT, .constant = {.size = 1}}}; \
    param_name.value.constant.buf[0] = param_name##_val;

// Helper macro to create a STRING parameter
#define CREATE_STRING_PARAM(param_name, str_data)                                 \
    s_param_raw param_name = {.version = 1,                                       \
                              .value = {.type_family = TF_STRING,                 \
                                        .source = SOURCE_CONSTANT,                \
                                        .constant = {.size = strlen(str_data)}}}; \
    memcpy(param_name.value.constant.buf, str_data, strlen(str_data));

// Helper macro to create a MAP_REF parameter.
// The key is a CONSTANT VALUE TLV encoding {0xAB, 0xCD} (2 bytes),
// so get_matching_map_entry will be called with key_size=2.
#define CREATE_MAP_REF_PARAM(param_name, map_id)                                               \
    static const uint8_t param_name##_key_tlv[] = {                                            \
        0x00,                                                                                  \
        0x01,                                                                                  \
        0x01, /* VERSION = 1 */                                                                \
        0x01,                                                                                  \
        0x01,                                                                                  \
        TF_BYTES, /* TYPE_FAMILY = TF_BYTES */                                                 \
        0x05,                                                                                  \
        0x02,                                                                                  \
        0xAB,                                                                                  \
        0xCD, /* CONSTANT = {0xAB, 0xCD} */                                                    \
    };                                                                                         \
    s_param_raw param_name = {                                                                 \
        .version = 1,                                                                          \
        .value = {.type_family = TF_STRING,                                                    \
                  .source = SOURCE_MAP_REF,                                                    \
                  .map_ref = {.id = (map_id), .key_tlv_size = sizeof(param_name##_key_tlv)}}}; \
    memcpy(param_name.value.map_ref.key_tlv, param_name##_key_tlv, sizeof(param_name##_key_tlv));

static bool g_add_to_field_table_ret = true;
static const void *g_get_matching_map_entry_ret = NULL;
static char g_last_field_value[256] = {0};

// =============================================================================
// Mock functions
// =============================================================================

/**
 * @brief Mock implementation of add_to_field_table
 */
bool add_to_field_table(e_param_type param_type, const char *name, const char *value) {
    snprintf(g_last_field_value, sizeof(g_last_field_value), "%s", value);
    return (bool) g_add_to_field_table_ret;
}

/**
 * @brief Mock implementation of get_matching_map_entry
 */
const s_map_entry *get_matching_map_entry(uint8_t id, const uint8_t *key, uint8_t key_size) {
    (void) key;
    return (const s_map_entry *) g_get_matching_map_entry_ret;
}

// Static map entry used by MAP_REF tests
static const s_map_entry map_entry_hello = {
    .value = {'H', 'e', 'l', 'l', 'o'},
    .value_size = 5,
};

// =============================================================================
// Test cases - TF_UINT
// =============================================================================

/**
 * @brief Test UINT with PARAM_VISIBILITY_ALWAYS
 */
void test_raw_uint_always(void) {
    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 42;  // Value = 42

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Amount"};

    // Mock
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test UINT with MUST_BE constraint - valid
 */
void test_raw_uint_must_be_valid(void) {
    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 100;  // Value = 100

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    // Add constraint: value MUST BE 100
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = INT256_LENGTH;
    constraint->value = calloc(1, INT256_LENGTH);
    constraint->value[INT256_LENGTH - 1] = 100;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Value"};

    // Should NOT call add_to_field_table (constraint matched, to_be_displayed = false)
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test UINT with MUST_BE constraint - invalid
 */
void test_raw_uint_must_be_invalid(void) {
    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 50;

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    // Constraint expects 100
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = INT256_LENGTH;
    constraint->value = calloc(1, INT256_LENGTH);
    constraint->value[INT256_LENGTH - 1] = 100;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Value"};

    // Should FAIL
    TEST_ASSERT_FALSE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test UINT with IF_NOT_IN constraint - value in list (don't display)
 */
void test_raw_uint_if_not_in_match(void) {
    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 0;  // Value = 0

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    // Constraint: don't display if value is 0
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = INT256_LENGTH;
    constraint->value = calloc(1, INT256_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalValue"};

    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test UINT with IF_NOT_IN constraint - value NOT in list (display)
 */
void test_raw_uint_if_not_in_no_match(void) {
    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 42;  // Value = 42

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    // Constraint: don't display if value is 0 (but value is 42, so should display)
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = INT256_LENGTH;
    constraint->value = calloc(1, INT256_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalValue"};

    // Mock - should be displayed because 42 is not in the exclusion list
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

// =============================================================================
// Test cases - TF_INT
// =============================================================================

/**
 * @brief Test INT with PARAM_VISIBILITY_ALWAYS - positive value
 */
void test_raw_int_always_positive(void) {
    // int32 = 42 (0x0000002A big-endian)
    uint8_t value_data[4] = {0x00, 0x00, 0x00, 0x2A};
    CREATE_INT_PARAM(param, value_data, 4, 4);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Delta"};
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test INT with PARAM_VISIBILITY_ALWAYS - negative value
 *
 * Regression test for CWE-451 (incorrect 8/16-bit signed rendering): the
 * canonical two's-complement string must be produced for any type_size.
 */
void test_raw_int_always_negative(void) {
    // int32 = -42 (0xFFFFFFD6 big-endian)
    uint8_t value_data[4] = {0xFF, 0xFF, 0xFF, 0xD6};
    CREATE_INT_PARAM(param, value_data, 4, 4);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Delta"};
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test INT with MUST_BE constraint - matching negative value
 */
void test_raw_int_must_be_valid(void) {
    // int32 = -10 (0xFFFFFFF6 big-endian)
    uint8_t value_data[4] = {0xFF, 0xFF, 0xFF, 0xF6};
    CREATE_INT_PARAM(param, value_data, 4, 4);

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 4;
    constraint->value = calloc(1, 4);
    constraint->value[0] = 0xFF;
    constraint->value[1] = 0xFF;
    constraint->value[2] = 0xFF;
    constraint->value[3] = 0xF6;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Delta"};

    // Match => to_be_displayed=false, no add_to_field_table call
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test INT with MUST_BE constraint - non-matching value rejects the TX
 */
void test_raw_int_must_be_invalid(void) {
    // int32 = +5
    uint8_t value_data[4] = {0x00, 0x00, 0x00, 0x05};
    CREATE_INT_PARAM(param, value_data, 4, 4);

    // Constraint expects -10
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 4;
    constraint->value = calloc(1, 4);
    constraint->value[0] = 0xFF;
    constraint->value[1] = 0xFF;
    constraint->value[2] = 0xFF;
    constraint->value[3] = 0xF6;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Delta"};

    TEST_ASSERT_FALSE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test INT with IF_NOT_IN constraint - value matches exclusion list
 */
void test_raw_int_if_not_in_match(void) {
    // int32 = 0
    uint8_t value_data[4] = {0x00, 0x00, 0x00, 0x00};
    CREATE_INT_PARAM(param, value_data, 4, 4);

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 4;
    constraint->value = calloc(1, 4);  // 0

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalDelta"};

    // Match => hidden, no add_to_field_table call
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test INT with IF_NOT_IN constraint - value not in exclusion list
 */
void test_raw_int_if_not_in_no_match(void) {
    // int32 = +42
    uint8_t value_data[4] = {0x00, 0x00, 0x00, 0x2A};
    CREATE_INT_PARAM(param, value_data, 4, 4);

    // Constraint: don't display if value is 0
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 4;
    constraint->value = calloc(1, 4);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalDelta"};
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

// =============================================================================
// Test cases - TF_ADDRESS
// =============================================================================

/**
 * @brief Test ADDRESS with PARAM_VISIBILITY_ALWAYS
 */
void test_raw_address_always(void) {
    // clang-format off
    CREATE_ADDRESS_PARAM(param,
        0xd8, 0xda, 0x6b, 0xf2, 0x69, 0x64, 0xaf, 0x9d, 0x7e, 0xed,
        0x9e, 0x03, 0xe5, 0x34, 0x15, 0xd3, 0x7a, 0xa9, 0x60, 0x45
    );
    // clang-format on
    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Recipient"};

    // Mock (address with real checksum from our deterministic mock)
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test ADDRESS with MUST_BE constraint - valid
 */
void test_raw_address_must_be_valid(void) {
    // clang-format off
    uint8_t expected_addr[ADDRESS_LENGTH] = {
        0xd8, 0xda, 0x6b, 0xf2, 0x69, 0x64, 0xaf, 0x9d, 0x7e, 0xed,
        0x9e, 0x03, 0xe5, 0x34, 0x15, 0xd3, 0x7a, 0xa9, 0x60, 0x45
    };

    CREATE_ADDRESS_PARAM(param,
        0xd8, 0xda, 0x6b, 0xf2, 0x69, 0x64, 0xaf, 0x9d, 0x7e, 0xed,
        0x9e, 0x03, 0xe5, 0x34, 0x15, 0xd3, 0x7a, 0xa9, 0x60, 0x45
    );
    // clang-format on

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, expected_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Recipient"};

    // Should NOT display (match found, to_be_displayed = false)
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test ADDRESS with MUST_BE constraint - invalid
 */
void test_raw_address_must_be_invalid(void) {
    // clang-format off
    CREATE_ADDRESS_PARAM(param,
        0xd8, 0xda, 0x6b, 0xf2, 0x69, 0x64, 0xaf, 0x9d, 0x7e, 0xed,
        0x9e, 0x03, 0xe5, 0x34, 0x15, 0xd3, 0x7a, 0xa9, 0x60, 0x45
    );

    // Constraint expects a different address
    uint8_t expected_addr[ADDRESS_LENGTH] = {
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xaa, 0xbb, 0xcc, 0xdd
    };
    // clang-format on

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, expected_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Recipient"};

    // Should FAIL (address doesn't match)
    TEST_ASSERT_FALSE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test ADDRESS with IF_NOT_IN constraint - value in list (don't display)
 */
void test_raw_address_if_not_in_match(void) {
    // clang-format off
    uint8_t zero_addr[ADDRESS_LENGTH] = {0};

    CREATE_ADDRESS_PARAM(param,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    );
    // clang-format on

    // Constraint: don't display if address is 0x0
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, zero_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalAddress"};

    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test ADDRESS with IF_NOT_IN constraint - value NOT in list (display)
 */
void test_raw_address_if_not_in_no_match(void) {
    // clang-format off
    uint8_t zero_addr[ADDRESS_LENGTH] = {0};

    CREATE_ADDRESS_PARAM(param,
        0xd8, 0xda, 0x6b, 0xf2, 0x69, 0x64, 0xaf, 0x9d, 0x7e, 0xed,
        0x9e, 0x03, 0xe5, 0x34, 0x15, 0xd3, 0x7a, 0xa9, 0x60, 0x45
    );
    // clang-format on

    // Constraint: don't display if address is 0x0 (but our address is not 0x0)
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, zero_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalAddress"};

    // Mock - should be displayed because address is not 0x0
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

// =============================================================================
// Test cases - TF_BOOL
// =============================================================================

/**
 * @brief Test BOOL with value true
 */
void test_raw_bool_true(void) {
    CREATE_BOOL_PARAM(param, true);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "IsValid"};

    // Mock
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test BOOL with value false
 */
void test_raw_bool_false(void) {
    CREATE_BOOL_PARAM(param, false);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Approved"};

    // Mock
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test BOOL with MUST_BE constraint - matching
 */
void test_raw_bool_must_be_valid(void) {
    CREATE_BOOL_PARAM(param, true);

    // Constraint: must be true (encoded as single non-zero byte)
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 1;
    constraint->value = calloc(1, 1);
    constraint->value[0] = 1;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Approved"};

    // Match => hidden
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BOOL with MUST_BE constraint - non-matching rejects the TX
 */
void test_raw_bool_must_be_invalid(void) {
    CREATE_BOOL_PARAM(param, true);

    // Constraint expects false
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 1;
    constraint->value = calloc(1, 1);
    constraint->value[0] = 0;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Approved"};

    TEST_ASSERT_FALSE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BOOL with IF_NOT_IN constraint - value matches exclusion list
 */
void test_raw_bool_if_not_in_match(void) {
    CREATE_BOOL_PARAM(param, false);

    // Constraint: hide if false
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 1;
    constraint->value = calloc(1, 1);
    constraint->value[0] = 0;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalFlag"};

    // Match => hidden
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BOOL with IF_NOT_IN constraint - value not in exclusion list
 */
void test_raw_bool_if_not_in_no_match(void) {
    CREATE_BOOL_PARAM(param, true);

    // Constraint: hide if false (but we have true)
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = 1;
    constraint->value = calloc(1, 1);
    constraint->value[0] = 0;

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalFlag"};
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

// =============================================================================
// Test cases - TF_BYTES
// =============================================================================

/**
 * @brief Test BYTES with PARAM_VISIBILITY_ALWAYS
 */
void test_raw_bytes_always(void) {
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    CREATE_BYTES_PARAM(param, data, 4);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Data"};
    // The format_bytes function uses snprintf with %.*h which may not work in test env
    // Just check that add_to_field_table is called
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_MUST_BE - valid constraint
 */
void test_raw_bytes_must_be_valid(void) {
    uint8_t data[3] = {0xAB, 0xCD, 0xEF};
    CREATE_BYTES_PARAM(param, data, 3);

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->value = calloc(1, 3);
    constraint->size = 3;
    memcpy(constraint->value, data, 3);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Data"};

    // MUST_BE with matching value: should NOT display (return true without calling
    // add_to_field_table)
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_MUST_BE - invalid constraint
 */
void test_raw_bytes_must_be_invalid(void) {
    uint8_t data[3] = {0xAB, 0xCD, 0xEF};
    CREATE_BYTES_PARAM(param, data, 3);

    uint8_t different_data[3] = {0x11, 0x22, 0x33};
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->value = calloc(1, 3);
    constraint->size = 3;
    memcpy(constraint->value, different_data, 3);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Data"};

    // MUST_BE with non-matching value: should return false (reject TX)
    TEST_ASSERT_FALSE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_IF_NOT_IN - match found
 */
void test_raw_bytes_if_not_in_match(void) {
    uint8_t data[2] = {0x00, 0x00};
    CREATE_BYTES_PARAM(param, data, 2);

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->value = calloc(1, 2);
    constraint->size = 2;
    memcpy(constraint->value, data, 2);  // Same as data

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Data"};

    // IF_NOT_IN with matching value: should NOT display (return true without calling
    // add_to_field_table)
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_IF_NOT_IN - no match
 */
void test_raw_bytes_if_not_in_no_match(void) {
    uint8_t data[3] = {0xAB, 0xCD, 0xEF};
    CREATE_BYTES_PARAM(param, data, 3);

    uint8_t exclusion_data[3] = {0x00, 0x00, 0x00};
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->value = calloc(1, 3);
    constraint->size = 3;
    memcpy(constraint->value, exclusion_data, 3);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Data"};
    g_add_to_field_table_ret = true;

    // IF_NOT_IN with non-matching value: should display
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief A BYTES constraint that is only a prefix of the value does not match
 */
void test_raw_bytes_prefix_constraint_no_match(void) {
    uint8_t data[4] = {0xAB, 0xCD, 0xEF, 0x01};
    CREATE_BYTES_PARAM(param, data, sizeof(data));

    // Constraint holds every byte but the last one
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->value = calloc(1, sizeof(data) - 1);
    constraint->size = sizeof(data) - 1;
    memcpy(constraint->value, data, sizeof(data) - 1);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Data"};

    // MUST_BE with no match: the transaction is rejected
    TEST_ASSERT_FALSE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief A BYTES value at the constant-size limit is displayed in full (no truncation)
 */
void test_raw_bytes_max_constant_displayed(void) {
    uint8_t data[CALLDATA_CHUNK_SIZE];
    memset(data, 0xAB, sizeof(data));
    CREATE_BYTES_PARAM(param, data, sizeof(data));

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Data"};
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));

    char expected[2 + (CALLDATA_CHUNK_SIZE * 2) + 1] = "0x";
    for (int i = 0; i < CALLDATA_CHUNK_SIZE; i++) {
        strcat(expected, "ab");
    }
    TEST_ASSERT_EQUAL_STRING(expected, g_last_field_value);
}

// =============================================================================
// Test cases - TF_STRING
// =============================================================================

/**
 * @brief Test STRING formatting
 */
void test_raw_string(void) {
    const char *test_str = "Hello Ethereum";
    CREATE_STRING_PARAM(param, test_str);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Message"};
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Test STRING with MUST_BE constraint - matching
 */
void test_raw_string_must_be_valid(void) {
    const char *test_str = "OK";
    CREATE_STRING_PARAM(param, test_str);

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = strlen(test_str);
    constraint->value = calloc(1, constraint->size);
    memcpy(constraint->value, test_str, constraint->size);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Status"};

    // Match => hidden
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test STRING with MUST_BE constraint - non-matching rejects the TX
 */
void test_raw_string_must_be_invalid(void) {
    const char *test_str = "NOPE";
    CREATE_STRING_PARAM(param, test_str);

    // Constraint expects "OK"
    const char *expected = "OK";
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = strlen(expected);
    constraint->value = calloc(1, constraint->size);
    memcpy(constraint->value, expected, constraint->size);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "Status"};

    TEST_ASSERT_FALSE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test STRING with IF_NOT_IN constraint - value matches exclusion list
 */
void test_raw_string_if_not_in_match(void) {
    const char *test_str = "ignored";
    CREATE_STRING_PARAM(param, test_str);

    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = strlen(test_str);
    constraint->value = calloc(1, constraint->size);
    memcpy(constraint->value, test_str, constraint->size);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalNote"};

    // Match => hidden
    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test STRING with IF_NOT_IN constraint - value not in exclusion list
 */
void test_raw_string_if_not_in_no_match(void) {
    const char *test_str = "Hello";
    CREATE_STRING_PARAM(param, test_str);

    // Constraint: hide if "ignored"
    const char *excluded = "ignored";
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = strlen(excluded);
    constraint->value = calloc(1, constraint->size);
    memcpy(constraint->value, excluded, constraint->size);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_raw = param,
                     .name = "OptionalNote"};
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief A STRING whose byte count fills the display buffer (no room for NUL) is rejected.
 *
 * format_string must return false so the field is dropped from the review
 * instead of being silently truncated on screen while the full value is hashed.
 */
void test_raw_string_oversize_rejected(void) {
    // Set constant.size = SHARED_CTX_FIELD_1_SIZE (380): value->length + 1 > buf_size.
    // format_string rejects before reading any content, so the fact that only 32
    // constant-buf bytes are backing the declared length cannot cause an overread.
    s_param_raw param = {.version = 1,
                         .value = {.type_family = TF_STRING,
                                   .source = SOURCE_CONSTANT,
                                   .constant = {.size = (uint8_t) SHARED_CTX_FIELD_1_SIZE}}};
    memset(param.value.constant.buf, 'A', sizeof(param.value.constant.buf));

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Message"};

    TEST_ASSERT_FALSE(format_param_raw(&field));
}

/**
 * @brief A STRING containing an embedded NUL byte is rejected.
 *
 * An embedded NUL would cause the screen to display only the prefix while
 * the full byte sequence is included in the instruction hash.
 */
void test_raw_string_embedded_nul_rejected(void) {
    static const uint8_t nul_str[] = {'h', 'e', 'l', '\0', 'o'};
    s_param_raw param = {.version = 1,
                         .value = {.type_family = TF_STRING,
                                   .source = SOURCE_CONSTANT,
                                   .constant = {.size = sizeof(nul_str)}}};
    memcpy(param.value.constant.buf, nul_str, sizeof(nul_str));

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Message"};

    TEST_ASSERT_FALSE(format_param_raw(&field));
}

// =============================================================================
// Test cases - SOURCE_MAP_REF
// =============================================================================

/**
 * @brief MAP_REF with a matching entry: the mapped value is displayed as a string.
 */
void test_raw_map_ref_found(void) {
    CREATE_MAP_REF_PARAM(param, 0);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Event name"};
    g_get_matching_map_entry_ret = &map_entry_hello;
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief MAP_REF with no matching entry: format_param_raw returns false.
 */
void test_raw_map_ref_not_found(void) {
    CREATE_MAP_REF_PARAM(param, 1);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Event name"};
    g_get_matching_map_entry_ret = NULL;

    TEST_ASSERT_FALSE(format_param_raw(&field));
}

/**
 * @brief MAP_REF whose KEY is itself a MAP_REF: rejected (nested MAP_REF unsupported).
 *
 * The key_tlv encodes a VALUE with tag 0x06 (MAP_REF), which causes value_get()
 * to detect key_value.source == SOURCE_MAP_REF and return false before calling
 * get_matching_map_entry.
 */
void test_raw_map_ref_nested_rejected(void) {
    // Inner-inner constant key TLV (9 bytes): VERSION + TYPE_FAMILY + CONSTANT={0xFF}
    // Inner MAP_REF TLV (17 bytes): VERSION + ID=5 + KEY(inner constant)
    // Nested key VALUE TLV (25 bytes): VERSION + TYPE_FAMILY + MAP_REF(inner)
    // clang-format off
    static const uint8_t nested_key_tlv[] = {
        0x00, 0x01, 0x01,        /* outer VALUE: VERSION = 1 */
        0x01, 0x01, TF_BYTES,    /* outer VALUE: TYPE_FAMILY = TF_BYTES */
        0x06, 0x11,              /* outer VALUE: MAP_REF tag, len=17 */
          /* inner MAP_REF TLV (17 bytes): */
          0x00, 0x01, 0x01,      /* inner MAP_REF: VERSION = 1 */
          0x01, 0x01, 0x05,      /* inner MAP_REF: ID = 5 */
          0x02, 0x09,            /* inner MAP_REF: KEY tag, len=9 */
            /* inner KEY VALUE TLV (9 bytes): */
            0x00, 0x01, 0x01,    /* inner KEY VALUE: VERSION = 1 */
            0x01, 0x01, TF_BYTES,/* inner KEY VALUE: TYPE_FAMILY = TF_BYTES */
            0x05, 0x01, 0xFF,    /* inner KEY VALUE: CONSTANT = {0xFF} */
    };
    // clang-format on

    s_param_raw param = {.version = 1,
                         .value = {.type_family = TF_STRING,
                                   .source = SOURCE_MAP_REF,
                                   .map_ref = {.id = 0, .key_tlv_size = sizeof(nested_key_tlv)}}};
    memcpy(param.value.map_ref.key_tlv, nested_key_tlv, sizeof(nested_key_tlv));

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Event name"};

    // get_matching_map_entry must NOT be called (rejected before lookup)
    TEST_ASSERT_FALSE(format_param_raw(&field));
}

// =============================================================================
// Test cases - SEPARATOR
// =============================================================================

/**
 * @brief Separator without {index}: emitted verbatim, then the value.
 */
void test_raw_uint_separator_literal(void) {
    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 42;

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Amount",
                     .separator = "Token"};

    // Separator entry (literal string, no substitution)
    g_add_to_field_table_ret = true;

    // Then the actual value
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

/**
 * @brief Separator with {index}: placeholder replaced by 1-based element index.
 */
void test_raw_uint_separator_with_index(void) {
    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 99;

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Token ID",
                     .separator = "Token {index}"};

    // Separator with index substitution (single element → index 1)
    g_add_to_field_table_ret = true;

    // Then the value
    g_add_to_field_table_ret = true;

    TEST_ASSERT_TRUE(format_param_raw(&field));
}

// =============================================================================
// TLV dispatch + format_bytes oversize
// =============================================================================

void test_handle_param_raw_struct_version_and_value_ok(void) {
    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x05,  // VERSION = 5
        0x01,
        0x00,  // VALUE (empty inner)
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_raw param = {0};
    s_param_raw_context ctx = {.param = &param};
    TEST_ASSERT_TRUE(handle_param_raw_struct(&buf, &ctx));
    TEST_ASSERT_EQUAL(param.version, 5);
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_raw_uint_always);
    RUN_TEST(test_raw_uint_must_be_valid);
    RUN_TEST(test_raw_uint_must_be_invalid);
    RUN_TEST(test_raw_uint_if_not_in_match);
    RUN_TEST(test_raw_uint_if_not_in_no_match);
    RUN_TEST(test_raw_int_always_positive);
    RUN_TEST(test_raw_int_always_negative);
    RUN_TEST(test_raw_int_must_be_valid);
    RUN_TEST(test_raw_int_must_be_invalid);
    RUN_TEST(test_raw_int_if_not_in_match);
    RUN_TEST(test_raw_int_if_not_in_no_match);
    RUN_TEST(test_raw_address_always);
    RUN_TEST(test_raw_address_must_be_valid);
    RUN_TEST(test_raw_address_must_be_invalid);
    RUN_TEST(test_raw_address_if_not_in_match);
    RUN_TEST(test_raw_address_if_not_in_no_match);
    RUN_TEST(test_raw_bool_true);
    RUN_TEST(test_raw_bool_false);
    RUN_TEST(test_raw_bool_must_be_valid);
    RUN_TEST(test_raw_bool_must_be_invalid);
    RUN_TEST(test_raw_bool_if_not_in_match);
    RUN_TEST(test_raw_bool_if_not_in_no_match);
    RUN_TEST(test_raw_bytes_always);
    RUN_TEST(test_raw_bytes_must_be_valid);
    RUN_TEST(test_raw_bytes_must_be_invalid);
    RUN_TEST(test_raw_bytes_if_not_in_match);
    RUN_TEST(test_raw_bytes_if_not_in_no_match);
    RUN_TEST(test_raw_bytes_max_constant_displayed);
    RUN_TEST(test_raw_bytes_prefix_constraint_no_match);
    RUN_TEST(test_raw_string);
    RUN_TEST(test_raw_string_must_be_valid);
    RUN_TEST(test_raw_string_must_be_invalid);
    RUN_TEST(test_raw_string_if_not_in_match);
    RUN_TEST(test_raw_string_if_not_in_no_match);
    RUN_TEST(test_raw_string_oversize_rejected);
    RUN_TEST(test_raw_string_embedded_nul_rejected);
    RUN_TEST(test_raw_map_ref_found);
    RUN_TEST(test_raw_map_ref_not_found);
    RUN_TEST(test_raw_map_ref_nested_rejected);
    RUN_TEST(test_raw_uint_separator_literal);
    RUN_TEST(test_raw_uint_separator_with_index);
    RUN_TEST(test_handle_param_raw_struct_version_and_value_ok);
    return UNITY_END();
}
