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
#include "buffer.h"
#include "tlv_library.h"

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
    s_field_ctx context = {.field = &field};

    // Simulate a CONSTRAINT tag with some data (e.g., a 4-byte value)
    // TLV format: tag(1 byte) + length(1 byte) + value(4 bytes)
    uint8_t tlv_data[] = {0x05, 0x04, 0x11, 0x22, 0x33, 0x44};  // TAG_CONSTRAINT
    buffer_t buf = {.ptr = tlv_data, .size = sizeof(tlv_data), .offset = 0};

    // Call handle_field_struct with CONSTRAINT but no VISIBLE set
    // This should fail because visibility must be set before constraints
    bool result = handle_field_struct(&buf, &context);

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
    s_field_ctx context = {.field = &field};

    // First, set VISIBLE to ALWAYS
    // TLV format: tag(0x04) + length(1) + value(0x00 = PARAM_VISIBILITY_ALWAYS)
    uint8_t visible_tlv[] = {0x04, 0x01, 0x00};
    buffer_t visible_buf = {.ptr = visible_tlv, .size = sizeof(visible_tlv), .offset = 0};

    bool result = handle_field_struct(&visible_buf, &context);
    assert_true(result);
    assert_int_equal(field.visibility, 0);  // PARAM_VISIBILITY_ALWAYS

    // Now try to add a CONSTRAINT
    uint8_t constraint_tlv[] = {0x05, 0x04, 0x11, 0x22, 0x33, 0x44};  // TAG_CONSTRAINT
    buffer_t constraint_buf = {.ptr = constraint_tlv, .size = sizeof(constraint_tlv), .offset = 0};

    result = handle_field_struct(&constraint_buf, &context);

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
    s_field_ctx context = {.field = &field};

    // Send both VISIBLE and CONSTRAINT in one TLV buffer
    // TLV format: VISIBLE + CONSTRAINT concatenated
    uint8_t tlv_data[] = {
        0x04,
        0x01,
        0x01,  // TAG_VISIBLE = PARAM_VISIBILITY_MUST_BE
        0x05,
        0x04,
        0x11,
        0x22,
        0x33,
        0x44  // TAG_CONSTRAINT with 4 bytes
    };
    buffer_t buf = {.ptr = tlv_data, .size = sizeof(tlv_data), .offset = 0};

    bool result = handle_field_struct(&buf, &context);

    // Should return true (both tags accepted)
    assert_true(result);
    assert_int_equal(field.visibility, 1);  // PARAM_VISIBILITY_MUST_BE

    // Verify constraint was added
    uint8_t expected_value[] = {0x11, 0x22, 0x33, 0x44};
    assert_non_null(field.constraints);
    assert_memory_equal(field.constraints->value, expected_value, 4);

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
    s_field_ctx context = {.field = &field};

    // Send both VISIBLE and CONSTRAINT in one TLV buffer
    uint8_t tlv_data[] = {
        0x04,
        0x01,
        0x02,  // TAG_VISIBLE = PARAM_VISIBILITY_IF_NOT_IN
        0x05,
        0x04,
        0x11,
        0x22,
        0x33,
        0x44  // TAG_CONSTRAINT with 4 bytes
    };
    buffer_t buf = {.ptr = tlv_data, .size = sizeof(tlv_data), .offset = 0};

    bool result = handle_field_struct(&buf, &context);

    // Should return true (both tags accepted)
    assert_true(result);
    assert_int_equal(field.visibility, 2);  // PARAM_VISIBILITY_IF_NOT_IN

    // Verify constraint was added
    uint8_t expected_value[] = {0x11, 0x22, 0x33, 0x44};
    assert_non_null(field.constraints);
    assert_memory_equal(field.constraints->value, expected_value, 4);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test multiple constraints can be added
 */
static void test_multiple_constraints_accepted(void **state) {
    (void) state;

    s_field field = {0};
    s_field_ctx context = {.field = &field};

    // Send VISIBLE + 2 CONSTRAINTs in one TLV buffer
    uint8_t tlv_data[] = {
        0x04,
        0x01,
        0x02,  // TAG_VISIBLE = PARAM_VISIBILITY_IF_NOT_IN
        0x05,
        0x04,
        0x11,
        0x22,
        0x33,
        0x44,  // TAG_CONSTRAINT 1
        0x05,
        0x04,
        0x55,
        0x66,
        0x77,
        0x88  // TAG_CONSTRAINT 2
    };
    buffer_t buf = {.ptr = tlv_data, .size = sizeof(tlv_data), .offset = 0};

    assert_true(handle_field_struct(&buf, &context));

    // Verify both constraints were added
    uint8_t constraint1_value[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t constraint2_value[] = {0x55, 0x66, 0x77, 0x88};

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
