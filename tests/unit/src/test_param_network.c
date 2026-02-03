/**
 * @file test_param_network.c
 * @brief Unit tests for network parameter formatting
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Includes
#include "gtp_field.h"
#include "gtp_value.h"
#include "shared_context.h"
#include "status_words.h"

// Headers for mocked functions
#include "network.h"
#include "common_utils.h"

strings_t strings;

// Helper macro to create a param_network with constant chain_id
#define CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param_name, chain_id_val)                  \
    uint8_t param_name##_data[8];                                                     \
    for (int i = 0; i < 8; i++) {                                                     \
        param_name##_data[7 - i] = (chain_id_val >> (i * 8)) & 0xFF;                  \
    }                                                                                 \
    s_param_network param_name = {.version = 1,                                       \
                                  .value = {.type_family = TF_UINT,                   \
                                            .source = SOURCE_CONSTANT,                \
                                            .constant = {.size = sizeof(uint64_t)}}}; \
    memcpy(param_name.value.constant.buf, param_name##_data, 8);

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

/**
 * @brief Mock implementation of get_network_as_string_from_chain_id
 */
uint16_t __wrap_get_network_as_string_from_chain_id(char *buffer,
                                                    size_t buffer_size,
                                                    uint64_t chain_id) {
    check_expected(chain_id);

    const char *network_name = (const char *) mock();
    if (network_name == NULL) {
        return SWO_PARAMETER_ERROR_NO_INFO;
    }

    strncpy(buffer, network_name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';

    return SWO_SUCCESS;
}

// =============================================================================
// Test cases
// =============================================================================

/**
 * @brief Test formatting Ethereum Mainnet (chain_id = 1)
 */
static void test_format_network_ethereum_mainnet(void **state) {
    (void) state;
    uint64_t chain_id = 1;
    const char *field_name = "Network";
    const char *network_name = "Ethereum";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);

    // Mock only external functions
    expect_value(__wrap_get_network_as_string_from_chain_id, chain_id, chain_id);
    will_return(__wrap_get_network_as_string_from_chain_id, network_name);

    expect_value(__wrap_add_to_field_table, param_type, PARAM_TYPE_NETWORK);
    expect_string(__wrap_add_to_field_table, name, field_name);
    expect_string(__wrap_add_to_field_table, value, network_name);
    will_return(__wrap_add_to_field_table, true);

    // Test
    assert_true(format_param_network(&param, field_name));
}

/**
 * @brief Test formatting Polygon (chain_id = 137)
 */
static void test_format_network_polygon(void **state) {
    (void) state;
    uint64_t chain_id = 137;
    const char *field_name = "Chain";
    const char *network_name = "Polygon";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);

    expect_value(__wrap_get_network_as_string_from_chain_id, chain_id, chain_id);
    will_return(__wrap_get_network_as_string_from_chain_id, network_name);

    expect_value(__wrap_add_to_field_table, param_type, PARAM_TYPE_NETWORK);
    expect_string(__wrap_add_to_field_table, name, field_name);
    expect_string(__wrap_add_to_field_table, value, network_name);
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_network(&param, field_name));
}

/**
 * @brief Test unsupported chain ID (returns error)
 */
static void test_format_network_unsupported(void **state) {
    (void) state;
    uint64_t chain_id = 99999;
    const char *field_name = "Network";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);

    expect_value(__wrap_get_network_as_string_from_chain_id, chain_id, chain_id);
    will_return(__wrap_get_network_as_string_from_chain_id, NULL);  // Unsupported

    assert_false(format_param_network(&param, field_name));
}

/**
 * @brief Test chain ID = 0 (invalid per EIP-2294)
 */
static void test_format_network_chain_id_zero(void **state) {
    (void) state;
    uint64_t chain_id = 0;
    const char *field_name = "Network";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);

    // Should fail - chain_id 0 is not valid
    assert_false(format_param_network(&param, field_name));
}

/**
 * @brief Test maximum valid chain ID per EIP-2294
 */
static void test_format_network_max_chain_id(void **state) {
    (void) state;
    const char *field_name = "Network";
    const char *network_name = "Unknown Chain";

    uint64_t max_chain_id = 0x7FFFFFFFFFFFFFDB;
    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, max_chain_id);

    expect_value(__wrap_get_network_as_string_from_chain_id, chain_id, max_chain_id);
    will_return(__wrap_get_network_as_string_from_chain_id, network_name);

    expect_value(__wrap_add_to_field_table, param_type, PARAM_TYPE_NETWORK);
    expect_string(__wrap_add_to_field_table, name, field_name);
    expect_string(__wrap_add_to_field_table, value, network_name);
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_network(&param, field_name));
}

/**
 * @brief Test chain ID exceeding maximum (invalid per EIP-2294)
 */
static void test_format_network_chain_id_overflow(void **state) {
    (void) state;
    const char *field_name = "Network";

    uint64_t invalid_chain_id = 0x7FFFFFFFFFFFFFDC;  // Max + 1
    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, invalid_chain_id);

    // Should fail - exceeds EIP-2294 maximum
    assert_false(format_param_network(&param, field_name));
}

/**
 * @brief Test wrong type family (not TF_UINT)
 */
static void test_format_network_wrong_type_family(void **state) {
    (void) state;
    uint64_t chain_id = 1;
    const char *field_name = "Network";

    CREATE_NETWORK_PARAM_WITH_CHAIN_ID(param, chain_id);
    param.value.type_family = TF_BYTES;  // Wrong type - should be TF_UINT

    // Should fail - wrong type family
    assert_false(format_param_network(&param, field_name));
}

// =============================================================================
// Test runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_format_network_ethereum_mainnet),
        cmocka_unit_test(test_format_network_polygon),
        cmocka_unit_test(test_format_network_unsupported),
        cmocka_unit_test(test_format_network_chain_id_zero),
        cmocka_unit_test(test_format_network_max_chain_id),
        cmocka_unit_test(test_format_network_chain_id_overflow),
        cmocka_unit_test(test_format_network_wrong_type_family),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
