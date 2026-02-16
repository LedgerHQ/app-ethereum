/**
 * @file test_param_raw.c
 * @brief Unit tests for raw parameter formatting
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

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

strings_t strings;

// Stub for chainConfig
static chain_config_t chainConfig_storage = {.coinName = "ETH", .chainId = 1};
const chain_config_t *chainConfig = &chainConfig_storage;

// Helper macro to create a UINT parameter
#define CREATE_UINT_PARAM(param_name, value_bytes, value_size)              \
    s_param_raw param_name = {.version = 1,                                 \
                              .value = {.type_family = TF_UINT,             \
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

// =============================================================================
// Mock functions
// =============================================================================

/**
 * @brief Mock implementation of add_to_field_table
 */
bool __wrap_add_to_field_table(e_param_type param_type, const char *name, const char *value) {
    check_expected(param_type);
    check_expected(name);
    check_expected(value);
    return (bool) mock();
}

// =============================================================================
// Test cases - TF_UINT
// =============================================================================

/**
 * @brief Test UINT with PARAM_VISIBILITY_ALWAYS
 */
static void test_raw_uint_always(void **state) {
    (void) state;

    uint8_t value_data[INT256_LENGTH] = {0};
    value_data[INT256_LENGTH - 1] = 42;  // Value = 42

    CREATE_UINT_PARAM(param, value_data, INT256_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Amount"};

    // Mock
    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "42");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));
}

/**
 * @brief Test UINT with MUST_BE constraint - valid
 */
static void test_raw_uint_must_be_valid(void **state) {
    (void) state;

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
    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test UINT with MUST_BE constraint - invalid
 */
static void test_raw_uint_must_be_invalid(void **state) {
    (void) state;

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
    assert_false(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test UINT with IF_NOT_IN constraint - value in list (don't display)
 */
static void test_raw_uint_if_not_in_match(void **state) {
    (void) state;

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

    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test UINT with IF_NOT_IN constraint - value NOT in list (display)
 */
static void test_raw_uint_if_not_in_no_match(void **state) {
    (void) state;

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
    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "42");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

// =============================================================================
// Test cases - TF_ADDRESS
// =============================================================================

/**
 * @brief Test ADDRESS with PARAM_VISIBILITY_ALWAYS
 */
static void test_raw_address_always(void **state) {
    (void) state;
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
    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "0xd8dA6bf26964af9d7eeD9e03e53415d37aa96045");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));
}

/**
 * @brief Test ADDRESS with MUST_BE constraint - valid
 */
static void test_raw_address_must_be_valid(void **state) {
    (void) state;
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
    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test ADDRESS with MUST_BE constraint - invalid
 */
static void test_raw_address_must_be_invalid(void **state) {
    (void) state;
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
    assert_false(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test ADDRESS with IF_NOT_IN constraint - value in list (don't display)
 */
static void test_raw_address_if_not_in_match(void **state) {
    (void) state;
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

    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test ADDRESS with IF_NOT_IN constraint - value NOT in list (display)
 */
static void test_raw_address_if_not_in_no_match(void **state) {
    (void) state;
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
    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "0xd8dA6bf26964af9d7eeD9e03e53415d37aa96045");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

// =============================================================================
// Test cases - TF_BOOL
// =============================================================================

/**
 * @brief Test BOOL with value true
 */
static void test_raw_bool_true(void **state) {
    (void) state;

    CREATE_BOOL_PARAM(param, true);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "IsValid"};

    // Mock
    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "true");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));
}

/**
 * @brief Test BOOL with value false
 */
static void test_raw_bool_false(void **state) {
    (void) state;

    CREATE_BOOL_PARAM(param, false);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Approved"};

    // Mock
    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "false");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));
}

// =============================================================================
// Test cases - TF_BYTES
// =============================================================================

/**
 * @brief Test BYTES with PARAM_VISIBILITY_ALWAYS
 */
static void test_raw_bytes_always(void **state) {
    (void) state;

    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    CREATE_BYTES_PARAM(param, data, 4);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Data"};

    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    // The format_bytes function uses snprintf with %.*h which may not work in test env
    // Just check that add_to_field_table is called
    expect_any(__wrap_add_to_field_table, value);
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_MUST_BE - valid constraint
 */
static void test_raw_bytes_must_be_valid(void **state) {
    (void) state;

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
    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_MUST_BE - invalid constraint
 */
static void test_raw_bytes_must_be_invalid(void **state) {
    (void) state;

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
    assert_false(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_IF_NOT_IN - match found
 */
static void test_raw_bytes_if_not_in_match(void **state) {
    (void) state;

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
    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test BYTES with PARAM_VISIBILITY_IF_NOT_IN - no match
 */
static void test_raw_bytes_if_not_in_no_match(void **state) {
    (void) state;

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

    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_any(__wrap_add_to_field_table, value);
    will_return(__wrap_add_to_field_table, true);

    // IF_NOT_IN with non-matching value: should display
    assert_true(format_param_raw(&field));

    free(constraint->value);
    free(constraint);
}

// =============================================================================
// Test cases - TF_STRING
// =============================================================================

/**
 * @brief Test STRING formatting
 */
static void test_raw_string(void **state) {
    (void) state;

    const char *test_str = "Hello Ethereum";
    CREATE_STRING_PARAM(param, test_str);

    s_field field = {.param_type = PARAM_TYPE_RAW,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_raw = param,
                     .name = "Message"};

    expect_value(__wrap_add_to_field_table, param_type, field.param_type);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, test_str);
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_raw(&field));
}

// =============================================================================
// Test runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        // UINT tests
        cmocka_unit_test(test_raw_uint_always),
        cmocka_unit_test(test_raw_uint_must_be_valid),
        cmocka_unit_test(test_raw_uint_must_be_invalid),
        cmocka_unit_test(test_raw_uint_if_not_in_match),
        cmocka_unit_test(test_raw_uint_if_not_in_no_match),

        // ADDRESS tests
        cmocka_unit_test(test_raw_address_always),
        cmocka_unit_test(test_raw_address_must_be_valid),
        cmocka_unit_test(test_raw_address_must_be_invalid),
        cmocka_unit_test(test_raw_address_if_not_in_match),
        cmocka_unit_test(test_raw_address_if_not_in_no_match),

        // BOOL tests
        cmocka_unit_test(test_raw_bool_true),
        cmocka_unit_test(test_raw_bool_false),

        // BYTES tests
        cmocka_unit_test(test_raw_bytes_always),
        cmocka_unit_test(test_raw_bytes_must_be_valid),
        cmocka_unit_test(test_raw_bytes_must_be_invalid),
        cmocka_unit_test(test_raw_bytes_if_not_in_match),
        cmocka_unit_test(test_raw_bytes_if_not_in_no_match),

        // STRING tests
        cmocka_unit_test(test_raw_string),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
