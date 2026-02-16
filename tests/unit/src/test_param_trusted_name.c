/**
 * @file test_param_trusted_name.c
 * @brief Unit tests for trusted name parameter formatting with constraints
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

#include "os.h"

// Headers for mocked functions
#include "trusted_name.h"
#include "utils.h"
#include "getPublicKey.h"
#include "tx_ctx.h"
#include "common_utils.h"

strings_t strings;

// Stub for chainConfig
static chain_config_t chainConfig_storage = {.coinName = "ETH", .chainId = 1};
const chain_config_t *chainConfig = &chainConfig_storage;

// Helper macro to create an Ethereum address (20 bytes)
#define CREATE_ADDRESS_PARAM(param_name, ...)                                           \
    uint8_t param_name##_addr[ADDRESS_LENGTH] = {__VA_ARGS__};                          \
    s_param_trusted_name param_name = {.version = 1,                                    \
                                       .value = {.type_family = TF_BYTES,               \
                                                 .source = SOURCE_CONSTANT,             \
                                                 .constant = {.size = ADDRESS_LENGTH}}, \
                                       .type_count = 1,                                 \
                                       .types = {TN_TYPE_ACCOUNT},                      \
                                       .source_count = 1,                               \
                                       .sources = {TN_SOURCE_CAL},                      \
                                       .sender_addr_count = 0};                         \
    memcpy(param_name.value.constant.buf, param_name##_addr, ADDRESS_LENGTH);

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
 * @brief Mock implementation of get_current_tx_chain_id
 */
uint64_t __wrap_get_current_tx_chain_id(void) {
    return (uint64_t) mock();
}

/**
 * @brief Mock implementation of get_trusted_name
 */
const s_trusted_name *__wrap_get_trusted_name(uint8_t type_count,
                                              const e_name_type *types,
                                              uint8_t source_count,
                                              const e_name_source *sources,
                                              const uint64_t *chain_id,
                                              const uint8_t *addr) {
    (void) type_count;
    (void) types;
    (void) source_count;
    (void) sources;

    check_expected_ptr(chain_id);
    check_expected_ptr(addr);

    return (const s_trusted_name *) mock();
}

/**
 * @brief Mock implementation of get_public_key
 */
uint16_t __wrap_get_public_key(uint8_t *out, size_t out_size) {
    check_expected(out_size);

    uint16_t status = (uint16_t) mock();
    if (status == SWO_SUCCESS) {
        // Fill with dummy wallet address
        // clang-format off
        uint8_t wallet_addr[ADDRESS_LENGTH] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11,
                                               0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                               0x99, 0x00, 0x11, 0x22, 0x33, 0x44};
        // clang-format on

        memcpy(out, wallet_addr, ADDRESS_LENGTH);
    }
    return status;
}

// =============================================================================
// Test cases
// =============================================================================

/**
 * @brief Test PARAM_VISIBILITY_ALWAYS with trusted name found
 */
static void test_trusted_name_always_with_trusted_name(void **state) {
    (void) state;

    // clang-format off
    // Create an address: 0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb
    CREATE_ADDRESS_PARAM(param,
        0x07, 0x42, 0xd3, 0x5C, 0xc6, 0x63, 0x4C, 0x05, 0x32, 0x92,
        0x5a, 0x3b, 0x84, 0x4B, 0xc9, 0xe7, 0x59, 0x5f, 0x0b, 0xEb
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};

    // Mock trusted name lookup
    will_return(__wrap_get_current_tx_chain_id, 1);

    static s_trusted_name trusted = {.name = "Vitalik.eth"};
    expect_any(__wrap_get_trusted_name, chain_id);
    expect_any(__wrap_get_trusted_name, addr);
    will_return(__wrap_get_trusted_name, &trusted);

    // Should add trusted name to field table
    expect_value(__wrap_add_to_field_table, param_type, PARAM_TYPE_TRUSTED_NAME);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, trusted.name);
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_trusted_name(&field));
}

/**
 * @brief Test PARAM_VISIBILITY_ALWAYS without trusted name (display raw address)
 */
static void test_trusted_name_always_without_trusted_name(void **state) {
    (void) state;

    // clang-format off
    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "From"};

    // Mock: no trusted name found
    will_return(__wrap_get_current_tx_chain_id, 1);

    expect_any(__wrap_get_trusted_name, chain_id);
    expect_any(__wrap_get_trusted_name, addr);
    will_return(__wrap_get_trusted_name, NULL);

    // Should display raw address (full checksummed address from real getEthDisplayableAddress)

    expect_value(__wrap_add_to_field_table, param_type, PARAM_TYPE_RAW);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "0x11223344556677889900aabbccddeEfF11223344");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_trusted_name(&field));
}

/**
 * @brief Test PARAM_VISIBILITY_MUST_BE with matching constraint (should NOT display)
 */
static void test_trusted_name_must_be_valid(void **state) {
    (void) state;

    // clang-format off
    uint8_t expected_addr[ADDRESS_LENGTH] = {0x07, 0x42, 0xd3, 0x5C, 0xc6, 0x63, 0x4C,
                                             0x05, 0x32, 0x92, 0x5a, 0x3b, 0x84, 0x4B,
                                             0xc9, 0xe7, 0x59, 0x5f, 0x0b, 0xEb};

    CREATE_ADDRESS_PARAM(param,
        0x07, 0x42, 0xd3, 0x5C, 0xc6, 0x63, 0x4C, 0x05, 0x32, 0x92,
        0x5a, 0x3b, 0x84, 0x4B, 0xc9, 0xe7, 0x59, 0x5f, 0x0b, 0xEb
    );
    // clang-format on

    // Add constraint: address MUST BE this specific address
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, expected_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Recipient"};

    // Mock
    will_return(__wrap_get_current_tx_chain_id, 1);

    static s_trusted_name trusted = {.name = "Treasury.eth"};
    expect_any(__wrap_get_trusted_name, chain_id);
    expect_any(__wrap_get_trusted_name, addr);
    will_return(__wrap_get_trusted_name, &trusted);

    // Should NOT call add_to_field_table (constraint matched, field hidden)

    assert_true(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test PARAM_VISIBILITY_MUST_BE with non-matching constraint (should reject TX)
 */
static void test_trusted_name_must_be_invalid(void **state) {
    (void) state;

    // clang-format off
    uint8_t expected_addr[ADDRESS_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add constraint: address MUST BE 0xFFFF...
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, expected_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_MUST_BE,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Recipient"};

    // Mock
    will_return(__wrap_get_current_tx_chain_id, 1);

    static s_trusted_name trusted = {.name = "Unknown.eth"};
    expect_any(__wrap_get_trusted_name, chain_id);
    expect_any(__wrap_get_trusted_name, addr);
    will_return(__wrap_get_trusted_name, &trusted);

    // Should FAIL because address doesn't match MUST_BE constraint
    assert_false(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test PARAM_VISIBILITY_IF_NOT_IN with address in constraint list (should NOT display)
 */
static void test_trusted_name_if_not_in_match(void **state) {
    (void) state;

    // clang-format off
    uint8_t blacklisted_addr[ADDRESS_LENGTH] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                                0x88, 0x99, 0x00, 0xaa, 0xbb, 0xcc, 0xdd,
                                                0xee, 0xff, 0x11, 0x22, 0x33, 0x44};

    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add constraint: display IF_NOT_IN this list (i.e., if address is in list, don't display)
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, blacklisted_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Spender"};

    // Mock
    will_return(__wrap_get_current_tx_chain_id, 1);

    static s_trusted_name trusted = {.name = "Blacklisted.eth"};
    expect_any(__wrap_get_trusted_name, chain_id);
    expect_any(__wrap_get_trusted_name, addr);
    will_return(__wrap_get_trusted_name, &trusted);

    // Should NOT call add_to_field_table (address is in IF_NOT_IN list)

    assert_true(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test PARAM_VISIBILITY_IF_NOT_IN with address NOT in constraint list (should display)
 */
static void test_trusted_name_if_not_in_no_match(void **state) {
    (void) state;

    // clang-format off
    uint8_t blacklisted_addr[ADDRESS_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                                0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add constraint: different address in blacklist
    s_field_constraint *constraint = calloc(1, sizeof(s_field_constraint));
    constraint->size = ADDRESS_LENGTH;
    constraint->value = malloc(ADDRESS_LENGTH);
    memcpy(constraint->value, blacklisted_addr, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_IF_NOT_IN,
                     .constraints = constraint,
                     .param_trusted_name = param,
                     .name = "Spender"};

    // Mock
    will_return(__wrap_get_current_tx_chain_id, 1);

    expect_any(__wrap_get_trusted_name, chain_id);
    expect_any(__wrap_get_trusted_name, addr);
    will_return(__wrap_get_trusted_name, NULL);

    // Should display raw address (not in blacklist, full checksummed address)

    expect_value(__wrap_add_to_field_table, param_type, PARAM_TYPE_RAW);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "0x11223344556677889900aabbccddeEfF11223344");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_trusted_name(&field));

    // Cleanup
    free(constraint->value);
    free(constraint);
}

/**
 * @brief Test with sender_addr replacement (wallet address)
 */
static void test_trusted_name_sender_addr_replacement(void **state) {
    (void) state;

    // clang-format off
    uint8_t sender_wallet[ADDRESS_LENGTH] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11,
                                             0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                             0x99, 0x00, 0x11, 0x22, 0x33, 0x44};

    CREATE_ADDRESS_PARAM(param,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    // Add sender address to param
    param.sender_addr_count = 1;
    memcpy(param.sender_addr[0], sender_wallet, ADDRESS_LENGTH);

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "From"};

    // Mock
    will_return(__wrap_get_current_tx_chain_id, 1);

    // Should replace with wallet address
    expect_value(__wrap_get_public_key, out_size, ADDRESS_LENGTH);
    will_return(__wrap_get_public_key, SWO_SUCCESS);

    // Mock trusted name lookup with replaced address
    expect_any(__wrap_get_trusted_name, chain_id);
    expect_any(__wrap_get_trusted_name, addr);
    will_return(__wrap_get_trusted_name, NULL);

    expect_value(__wrap_add_to_field_table, param_type, PARAM_TYPE_RAW);
    expect_string(__wrap_add_to_field_table, name, field.name);
    expect_string(__wrap_add_to_field_table, value, "0xaabbccddeeff1122334455667788990011223344");
    will_return(__wrap_add_to_field_table, true);

    assert_true(format_param_trusted_name(&field));
}

/**
 * @brief Test with chain_id = 0 (should fail)
 */
static void test_trusted_name_chain_id_zero(void **state) {
    (void) state;

    // clang-format off
    CREATE_ADDRESS_PARAM(param,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22, 0x33, 0x44
    );
    // clang-format on

    s_field field = {.param_type = PARAM_TYPE_TRUSTED_NAME,
                     .visibility = PARAM_VISIBILITY_ALWAYS,
                     .constraints = NULL,
                     .param_trusted_name = param,
                     .name = "To"};

    // Mock: chain_id = 0 is invalid
    will_return(__wrap_get_current_tx_chain_id, 0);

    assert_false(format_param_trusted_name(&field));
}

// =============================================================================
// Test runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_trusted_name_always_with_trusted_name),
        cmocka_unit_test(test_trusted_name_always_without_trusted_name),
        cmocka_unit_test(test_trusted_name_must_be_valid),
        cmocka_unit_test(test_trusted_name_must_be_invalid),
        cmocka_unit_test(test_trusted_name_if_not_in_match),
        cmocka_unit_test(test_trusted_name_if_not_in_no_match),
        cmocka_unit_test(test_trusted_name_sender_addr_replacement),
        cmocka_unit_test(test_trusted_name_chain_id_zero),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
