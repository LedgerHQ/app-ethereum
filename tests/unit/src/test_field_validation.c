/**
 * @file test_field_validation.c
 * @brief Unit tests for field struct validation rules
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include <cmocka.h>

#include "gtp_field.h"
#include "shared_context.h"
#include "mem.h"

// External dependencies
strings_t strings;
static chain_config_t chainConfig_storage = {.coinName = "ETH", .chainId = 1};
const chain_config_t *chainConfig = &chainConfig_storage;

// =============================================================================
// Test Cases
// =============================================================================

/**
 * @brief Test that CONSTRAINT without VISIBLE is rejected
 *
 * According to handle_param_constraint(), a constraint cannot be added
 * if the VISIBLE field has not been set yet.
 */
static void test_constraint_without_visible_rejected(void **state) {
    (void) state;

    s_field field = {0};
    s_field_ctx context = {.field = &field, .set_flags = 0};

    // Simulate a CONSTRAINT tag with some data (e.g., a 4-byte value)
    uint8_t constraint_value[4] = {0x11, 0x22, 0x33, 0x44};
    s_tlv_data constraint_data = {.tag = 0x05,  // CONSTRAINT tag
                                  .length = 4,
                                  .value = constraint_value};

    // Call handle_field_struct with CONSTRAINT but no VISIBLE set
    // This should fail because visibility must be set before constraints
    bool result = handle_field_struct(&constraint_data, &context);

    // Should return false (constraint rejected)
    assert_false(result);

    // Verify no constraint was added
    assert_null(field.constraints);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test that CONSTRAINT with VISIBLE=ALWAYS is rejected
 *
 * Constraints are only valid with MUST_BE or IF_NOT_IN visibility.
 */
static void test_constraint_with_always_visibility_rejected(void **state) {
    (void) state;

    s_field field = {0};
    s_field_ctx context = {.field = &field, .set_flags = 0};

    // First, set VISIBLE to ALWAYS
    uint8_t visible_value = 0x00;  // PARAM_VISIBILITY_ALWAYS = 0
    s_tlv_data visible_data = {
        .tag = 0x04,  // VISIBLE tag
        .length = 1,  // 1 byte on Ledger platform (enums are 1 byte with -fshort-enums)
        .value = &visible_value};

    bool result = handle_field_struct(&visible_data, &context);
    assert_true(result);
    assert_int_equal(field.visibility, 0);  // PARAM_VISIBILITY_ALWAYS

    // Now try to add a CONSTRAINT
    uint8_t constraint_value[4] = {0x11, 0x22, 0x33, 0x44};
    s_tlv_data constraint_data = {.tag = 0x05,  // CONSTRAINT tag
                                  .length = 4,
                                  .value = constraint_value};

    result = handle_field_struct(&constraint_data, &context);

    // Should return false (constraint not allowed with ALWAYS visibility)
    assert_false(result);

    // Verify no constraint was added
    assert_null(field.constraints);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test that CONSTRAINT with VISIBLE=MUST_BE is accepted
 *
 * Constraints are valid with MUST_BE visibility.
 */
static void test_constraint_with_must_be_visibility_accepted(void **state) {
    (void) state;

    s_field field = {0};
    s_field_ctx context = {.field = &field, .set_flags = 0};

    // Set VISIBLE to MUST_BE
    uint8_t visible_value = 0x01;  // PARAM_VISIBILITY_MUST_BE = 1
    s_tlv_data visible_data = {
        .tag = 0x04,  // VISIBLE tag
        .length = 1,  // 1 byte on Ledger platform (enums are 1 byte with -fshort-enums)
        .value = &visible_value};

    bool result = handle_field_struct(&visible_data, &context);
    assert_true(result);
    assert_int_equal(field.visibility, 1);  // PARAM_VISIBILITY_MUST_BE

    // Now add a CONSTRAINT - should succeed
    uint8_t constraint_value[4] = {0x11, 0x22, 0x33, 0x44};
    s_tlv_data constraint_data = {.tag = 0x05,  // CONSTRAINT tag
                                  .length = 4,
                                  .value = constraint_value};

    result = handle_field_struct(&constraint_data, &context);

    // Should return true (constraint accepted)
    assert_true(result);

    // Verify constraint was added
    assert_non_null(field.constraints);
    assert_memory_equal(field.constraints->value, constraint_value, 4);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test that CONSTRAINT with VISIBLE=IF_NOT_IN is accepted
 *
 * Constraints are valid with IF_NOT_IN visibility.
 */
static void test_constraint_with_if_not_in_visibility_accepted(void **state) {
    (void) state;

    s_field field = {0};
    s_field_ctx context = {.field = &field, .set_flags = 0};

    // Set VISIBLE to IF_NOT_IN
    uint8_t visible_value = 0x02;  // PARAM_VISIBILITY_IF_NOT_IN = 2
    s_tlv_data visible_data = {
        .tag = 0x04,  // VISIBLE tag
        .length = 1,  // 1 byte on Ledger platform (enums are 1 byte with -fshort-enums)
        .value = &visible_value};

    bool result = handle_field_struct(&visible_data, &context);
    assert_true(result);
    assert_int_equal(field.visibility, 2);  // PARAM_VISIBILITY_IF_NOT_IN

    // Now add a CONSTRAINT - should succeed
    uint8_t constraint_value[4] = {0x11, 0x22, 0x33, 0x44};
    s_tlv_data constraint_data = {.tag = 0x05,  // CONSTRAINT tag
                                  .length = 4,
                                  .value = constraint_value};

    result = handle_field_struct(&constraint_data, &context);

    // Should return true (constraint accepted)
    assert_true(result);

    // Verify constraint was added
    assert_non_null(field.constraints);
    assert_memory_equal(field.constraints->value, constraint_value, 4);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test multiple constraints can be added
 */
static void test_multiple_constraints_accepted(void **state) {
    (void) state;

    s_field field = {0};
    s_field_ctx context = {.field = &field, .set_flags = 0};

    // Set VISIBLE to IF_NOT_IN
    uint8_t visible_value = 0x02;  // PARAM_VISIBILITY_IF_NOT_IN = 2
    s_tlv_data visible_data = {
        .tag = 0x04,
        .length = 1,  // 1 byte on Ledger platform (enums are 1 byte with -fshort-enums)
        .value = &visible_value};

    assert_true(handle_field_struct(&visible_data, &context));

    // Add first constraint
    uint8_t constraint1_value[4] = {0x11, 0x22, 0x33, 0x44};
    s_tlv_data constraint1_data = {.tag = 0x05, .length = 4, .value = constraint1_value};

    assert_true(handle_field_struct(&constraint1_data, &context));

    // Add second constraint
    uint8_t constraint2_value[4] = {0x55, 0x66, 0x77, 0x88};
    s_tlv_data constraint2_data = {.tag = 0x05, .length = 4, .value = constraint2_value};

    assert_true(handle_field_struct(&constraint2_data, &context));

    // Verify both constraints were added
    assert_non_null(field.constraints);
    assert_memory_equal(field.constraints->value, constraint1_value, 4);

    s_field_constraint *second = (s_field_constraint *) field.constraints->node.next;
    assert_non_null(second);
    assert_memory_equal(second->value, constraint2_value, 4);

    // Cleanup
    cleanup_field_constraints(&field);
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_constraint_without_visible_rejected),
        cmocka_unit_test(test_constraint_with_always_visibility_rejected),
        cmocka_unit_test(test_constraint_with_must_be_visibility_accepted),
        cmocka_unit_test(test_constraint_with_if_not_in_visibility_accepted),
        cmocka_unit_test(test_multiple_constraints_accepted),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
