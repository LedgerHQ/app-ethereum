/**
 * @file test_field_validation.c
 * @brief Unit tests for field struct validation rules
 */

#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "unity.h"
#include "gtp_field.h"
#include "shared_context.h"
#include "buffer.h"
#include "tlv_library.h"

// =============================================================================
// Test Cases
// =============================================================================

/**
 * @brief Test that CONSTRAINT without VISIBLE is rejected
 *
 * According to handle_param_constraint(), a constraint cannot be added
 * if the VISIBLE field has not been set yet.
 */
void test_constraint_without_visible_rejected(void) {
    s_field field = {0};
    s_field_ctx context = {.field = &field};

    // Simulate a CONSTRAINT tag with some data (e.g., a 4-byte value)
    // TLV format: tag(1 byte) + length(1 byte) + value(4 bytes)
    uint8_t tlv_data[] = {0x00, 0x01, 0x01, 0x05, 0x04, 0x11, 0x22, 0x33, 0x44};
    buffer_t buf = {.ptr = tlv_data, .size = sizeof(tlv_data), .offset = 0};

    // Call handle_field_struct with CONSTRAINT but no VISIBLE set
    // This should fail because visibility must be set before constraints
    bool result = handle_field_struct(&buf, &context);

    // Should return false (constraint rejected)
    TEST_ASSERT_FALSE(result);

    // Verify no constraint was added
    TEST_ASSERT_NULL(field.constraints);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test that CONSTRAINT with VISIBLE=ALWAYS is rejected
 *
 * Constraints are only valid with MUST_BE or IF_NOT_IN visibility.
 */
void test_constraint_with_always_visibility_rejected(void) {
    s_field field = {0};
    s_field_ctx context = {.field = &field};

    // First, set VISIBLE to ALWAYS
    // TLV format: tag(0x04) + length(1) + value(0x00 = PARAM_VISIBILITY_ALWAYS)
    uint8_t visible_tlv[] = {0x00, 0x01, 0x01, 0x04, 0x01, 0x00};
    buffer_t visible_buf = {.ptr = visible_tlv, .size = sizeof(visible_tlv), .offset = 0};

    bool result = handle_field_struct(&visible_buf, &context);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(field.visibility, 0);  // PARAM_VISIBILITY_ALWAYS

    // Now try to add a CONSTRAINT
    uint8_t constraint_tlv[] = {0x05, 0x04, 0x11, 0x22, 0x33, 0x44};  // TAG_CONSTRAINT
    buffer_t constraint_buf = {.ptr = constraint_tlv, .size = sizeof(constraint_tlv), .offset = 0};

    result = handle_field_struct(&constraint_buf, &context);

    // Should return false (constraint not allowed with ALWAYS visibility)
    TEST_ASSERT_FALSE(result);

    // Verify no constraint was added
    TEST_ASSERT_NULL(field.constraints);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test that CONSTRAINT with VISIBLE=MUST_BE is accepted
 *
 * Constraints are valid with MUST_BE visibility.
 */
void test_constraint_with_must_be_visibility_accepted(void) {
    s_field field = {0};
    s_field_ctx context = {.field = &field};

    // Send both VISIBLE and CONSTRAINT in one TLV buffer
    // TLV format: VISIBLE + CONSTRAINT concatenated
    uint8_t tlv_data[] = {
        0x00,
        0x01,
        0x01,  // TAG_VERSION = 1
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
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(field.visibility, 1);  // PARAM_VISIBILITY_MUST_BE

    // Verify constraint was added
    uint8_t expected_value[] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_NOT_NULL(field.constraints);
    TEST_ASSERT_EQUAL_MEMORY(field.constraints->value, expected_value, 4);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test that CONSTRAINT with VISIBLE=IF_NOT_IN is accepted
 *
 * Constraints are valid with IF_NOT_IN visibility.
 */
void test_constraint_with_if_not_in_visibility_accepted(void) {
    s_field field = {0};
    s_field_ctx context = {.field = &field};

    // Send both VISIBLE and CONSTRAINT in one TLV buffer
    uint8_t tlv_data[] = {
        0x00,
        0x01,
        0x01,  // TAG_VERSION = 1
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
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(field.visibility, 2);  // PARAM_VISIBILITY_IF_NOT_IN

    // Verify constraint was added
    uint8_t expected_value[] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_NOT_NULL(field.constraints);
    TEST_ASSERT_EQUAL_MEMORY(field.constraints->value, expected_value, 4);

    // Cleanup
    cleanup_field_constraints(&field);
}

/**
 * @brief Test multiple constraints can be added
 */
void test_multiple_constraints_accepted(void) {
    s_field field = {0};
    s_field_ctx context = {.field = &field};

    // Send VISIBLE + 2 CONSTRAINTs in one TLV buffer
    uint8_t tlv_data[] = {
        0x00,
        0x01,
        0x01,  // TAG_VERSION = 1
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

    TEST_ASSERT_TRUE(handle_field_struct(&buf, &context));

    // Verify both constraints were added
    uint8_t constraint1_value[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t constraint2_value[] = {0x55, 0x66, 0x77, 0x88};

    TEST_ASSERT_NOT_NULL(field.constraints);
    TEST_ASSERT_EQUAL_MEMORY(field.constraints->value, constraint1_value, 4);

    s_field_constraint *second = (s_field_constraint *) field.constraints->node.next;
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_EQUAL_MEMORY(second->value, constraint2_value, 4);

    // Cleanup
    cleanup_field_constraints(&field);
}

// =============================================================================
// Per-tag dispatch tests — pin the type / visibility / param-ordering
// rules that gtp_field.c enforces on attacker-controlled GCS field
// descriptors.
// =============================================================================

// Drive handle_field_struct with a single tag at a time so failures
// are easy to attribute.
static bool run_field_tlv(const uint8_t *bytes, size_t size, s_field_ctx *ctx) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    return handle_field_struct(&buf, ctx);
}

// Build the minimum required TLV envelope: VERSION + NAME + PARAM_TYPE
// + PARAM. The PARAM payload is empty — the corresponding stub returns
// true, so the dispatch lands on a no-op.
static const uint8_t g_minimal_required_tlv[] = {
    0x00,
    0x01,
    0x01,  // VERSION = 1
    0x01,
    0x04,
    'A',
    'm',
    'o',
    'u',  // NAME = "Amou"
    0x02,
    0x01,
    0x00,  // PARAM_TYPE = PARAM_TYPE_RAW
    0x03,
    0x00,  // PARAM (empty payload)
};

void test_verify_field_happy_path_with_defaulted_visibility(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    TEST_ASSERT_TRUE(run_field_tlv(g_minimal_required_tlv, sizeof(g_minimal_required_tlv), &ctx));
    TEST_ASSERT_TRUE(verify_field_struct(&ctx));
    // VISIBLE not set → must default to ALWAYS.
    TEST_ASSERT_EQUAL(field.visibility, 0);  // PARAM_VISIBILITY_ALWAYS
    cleanup_field_constraints(&field);
}

void test_verify_field_missing_version_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    // Empty TLV: no VERSION received → verify rejects.
    TEST_ASSERT_TRUE(run_field_tlv(NULL, 0, &ctx));
    TEST_ASSERT_FALSE(verify_field_struct(&ctx));
}

void test_verify_field_unsupported_version_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    const uint8_t bytes[] = {0x00, 0x01, 0x07};  // VERSION = 7 (unsupported)
    TEST_ASSERT_TRUE(run_field_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_FALSE(verify_field_struct(&ctx));
}

void test_verify_field_missing_name_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    // VERSION + PARAM_TYPE + PARAM but no NAME.
    const uint8_t bytes[] = {0x00, 0x01, 0x01, 0x02, 0x01, 0x00, 0x03, 0x00};
    TEST_ASSERT_TRUE(run_field_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_FALSE(verify_field_struct(&ctx));
}

void test_field_version_not_first_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    // NAME before VERSION: VERSION must be the first tag, else payloads could be reframed.
    const uint8_t bytes[] = {0x01, 0x01, 'A', 0x00, 0x01, 0x01};
    TEST_ASSERT_FALSE(run_field_tlv(bytes, sizeof(bytes), &ctx));
}

void test_handle_param_type_unsupported_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    // PARAM_TYPE = 0xFF — outside every case branch.
    const uint8_t bytes[] = {0x02, 0x01, 0xFF};
    TEST_ASSERT_FALSE(run_field_tlv(bytes, sizeof(bytes), &ctx));
}

void test_handle_param_visible_out_of_range_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    // VISIBLE >= PARAM_VISIBILITY_MAX must be rejected — otherwise a
    // host-controlled value could land in a downstream switch default
    // and silently change display behavior.
    const uint8_t bytes[] = {0x04, 0x01, 0xFF};
    TEST_ASSERT_FALSE(run_field_tlv(bytes, sizeof(bytes), &ctx));
}

void test_handle_param_without_param_type_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    // PARAM tag arrives before PARAM_TYPE → must be rejected so the
    // dispatch can't land on whichever default-zero param_type is sitting
    // in the struct.
    const uint8_t bytes[] = {0x03, 0x00};
    TEST_ASSERT_FALSE(run_field_tlv(bytes, sizeof(bytes), &ctx));
}

void test_constraint_empty_value_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    const uint8_t bytes[] = {
        0x04,
        0x01,
        0x01,  // VISIBLE = MUST_BE
        0x05,
        0x00,  // CONSTRAINT with empty value
    };
    TEST_ASSERT_FALSE(run_field_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_NULL(field.constraints);
}

// =============================================================================
// handle_param dispatch — every PARAM_TYPE has its own handle_param_X_struct
// hop and the field_validation_mocks stubs always return true. The point
// is to confirm the switch lands on the right helper for each type
// rather than silently falling through to the default reject case (which
// would let a host-controlled descriptor pick *no* handler — bypassing
// the per-type size/range checks done inside the corresponding parser).
// =============================================================================

static void run_param_dispatch_for_type(uint8_t param_type) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x02,
        0x01,
        param_type,  // PARAM_TYPE
        0x03,
        0x00,  // PARAM (empty payload)
    };
    TEST_ASSERT_TRUE(run_field_tlv(bytes, sizeof(bytes), &ctx));
    cleanup_field_constraints(&field);
}

void test_handle_param_dispatches_raw(void) {
    run_param_dispatch_for_type(PARAM_TYPE_RAW);
}
void test_handle_param_dispatches_amount(void) {
    run_param_dispatch_for_type(PARAM_TYPE_AMOUNT);
}
void test_handle_param_dispatches_token_amount(void) {
    run_param_dispatch_for_type(PARAM_TYPE_TOKEN_AMOUNT);
}
void test_handle_param_dispatches_nft(void) {
    run_param_dispatch_for_type(PARAM_TYPE_NFT);
}
void test_handle_param_dispatches_datetime(void) {
    run_param_dispatch_for_type(PARAM_TYPE_DATETIME);
}
void test_handle_param_dispatches_duration(void) {
    run_param_dispatch_for_type(PARAM_TYPE_DURATION);
}
void test_handle_param_dispatches_unit(void) {
    run_param_dispatch_for_type(PARAM_TYPE_UNIT);
}
void test_handle_param_dispatches_enum(void) {
    run_param_dispatch_for_type(PARAM_TYPE_ENUM);
}
void test_handle_param_dispatches_trusted_name(void) {
    run_param_dispatch_for_type(PARAM_TYPE_TRUSTED_NAME);
}
void test_handle_param_dispatches_calldata(void) {
    run_param_dispatch_for_type(PARAM_TYPE_CALLDATA);
}
void test_handle_param_dispatches_token(void) {
    run_param_dispatch_for_type(PARAM_TYPE_TOKEN);
}
void test_handle_param_dispatches_network(void) {
    run_param_dispatch_for_type(PARAM_TYPE_NETWORK);
}
void test_handle_param_dispatches_group(void) {
    run_param_dispatch_for_type(PARAM_TYPE_GROUP);
}

// =============================================================================
// format_field dispatch — same surface as above, but for the render side.
// Each branch calls a format_param_X stub that returns true.
// =============================================================================

static void run_format_for_type(uint8_t param_type) {
    s_field field = {0};
    field.version = 1;
    field.param_type = param_type;
    TEST_ASSERT_TRUE(format_field(&field, 0));
}

void test_format_field_raw(void) {
    run_format_for_type(PARAM_TYPE_RAW);
}
void test_format_field_amount(void) {
    run_format_for_type(PARAM_TYPE_AMOUNT);
}
void test_format_field_token_amount(void) {
    run_format_for_type(PARAM_TYPE_TOKEN_AMOUNT);
}
void test_format_field_nft(void) {
    run_format_for_type(PARAM_TYPE_NFT);
}
void test_format_field_datetime(void) {
    run_format_for_type(PARAM_TYPE_DATETIME);
}
void test_format_field_duration(void) {
    run_format_for_type(PARAM_TYPE_DURATION);
}
void test_format_field_unit(void) {
    run_format_for_type(PARAM_TYPE_UNIT);
}
void test_format_field_enum(void) {
    run_format_for_type(PARAM_TYPE_ENUM);
}
void test_format_field_trusted_name(void) {
    run_format_for_type(PARAM_TYPE_TRUSTED_NAME);
}
void test_format_field_calldata(void) {
    run_format_for_type(PARAM_TYPE_CALLDATA);
}
void test_format_field_token(void) {
    run_format_for_type(PARAM_TYPE_TOKEN);
}
void test_format_field_network(void) {
    run_format_for_type(PARAM_TYPE_NETWORK);
}
void test_format_field_group(void) {
    run_format_for_type(PARAM_TYPE_GROUP);
}

void test_format_field_unsupported_type_rejected(void) {
    s_field field = {0};
    field.version = 1;
    field.param_type = 0xFE;  // outside every case
    TEST_ASSERT_FALSE(format_field(&field, 0));
}

// =============================================================================
// cleanup_field — NULL-safe + tears down both group state and constraints.
// =============================================================================

void test_cleanup_field_null_safe(void) {
    cleanup_field(NULL);  // no crash
}

void test_cleanup_field_with_group_routes_through_group_cleanup(void) {
    s_field field = {0};
    field.param_type = PARAM_TYPE_GROUP;
    // No constraints, no real group nodes; mock cleanup_param_group is a
    // no-op. The point is to land on the GROUP branch of cleanup_field.
    cleanup_field(&field);
}

// =============================================================================
// handle_separator — populates the field's separator string.
// =============================================================================

// The first `test_constraint_with_always_visibility_rejected` actually
// hits the "no VISIBLE received in this call" branch (the TLV parser
// resets received_tags on every entry, so a CONSTRAINT in a separate
// call doesn't see the earlier VISIBLE). To exercise the
// visibility==ALWAYS rejection branch we need VISIBLE + CONSTRAINT
// in a single buffer.
void test_constraint_with_always_visibility_single_call_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    const uint8_t bytes[] = {
        0x04,
        0x01,
        0x00,  // VISIBLE = ALWAYS
        0x05,
        0x04,
        0x11,
        0x22,
        0x33,
        0x44,  // CONSTRAINT (illegal with ALWAYS)
    };
    TEST_ASSERT_FALSE(run_field_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_NULL(field.constraints);
}

// node->size is uint8_t; a constraint value > 255 bytes would
// silently truncate the stored size if the size guard were removed,
// making the later memcmp-based constraint check always fail.
// Send a 256-byte constraint to pin the explicit rejection.
void test_constraint_value_above_uint8_max_rejected(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};

    // TLV: VISIBLE + CONSTRAINT(256 bytes). TLV length 256 needs
    // DER long-form: 0x82 0x01 0x00.
    uint8_t bytes[3 + 4 + 256];
    bytes[0] = 0x04;  // VISIBLE
    bytes[1] = 0x01;
    bytes[2] = 0x01;  // MUST_BE
    bytes[3] = 0x05;  // CONSTRAINT
    bytes[4] = 0x82;  // length long-form, 2 bytes follow
    bytes[5] = 0x01;
    bytes[6] = 0x00;  // length = 0x0100 = 256
    memset(bytes + 7, 0xCC, 256);
    TEST_ASSERT_FALSE(run_field_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_NULL(field.constraints);
}

void test_handle_separator_populates_field(void) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x06,
        0x03,
        ' ',
        '|',
        ' ',  // SEPARATOR = " | "
    };
    TEST_ASSERT_TRUE(run_field_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_EQUAL_STRING(field.separator, " | ");
}

// =============================================================================
// Main Test Runner
// =============================================================================

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_constraint_without_visible_rejected);
    RUN_TEST(test_constraint_with_always_visibility_rejected);
    RUN_TEST(test_constraint_with_must_be_visibility_accepted);
    RUN_TEST(test_constraint_with_if_not_in_visibility_accepted);
    RUN_TEST(test_multiple_constraints_accepted);
    RUN_TEST(test_verify_field_happy_path_with_defaulted_visibility);
    RUN_TEST(test_verify_field_missing_version_rejected);
    RUN_TEST(test_verify_field_unsupported_version_rejected);
    RUN_TEST(test_verify_field_missing_name_rejected);
    RUN_TEST(test_field_version_not_first_rejected);
    RUN_TEST(test_handle_param_type_unsupported_rejected);
    RUN_TEST(test_handle_param_visible_out_of_range_rejected);
    RUN_TEST(test_handle_param_without_param_type_rejected);
    RUN_TEST(test_constraint_empty_value_rejected);
    RUN_TEST(test_handle_param_dispatches_raw);
    RUN_TEST(test_handle_param_dispatches_amount);
    RUN_TEST(test_handle_param_dispatches_token_amount);
    RUN_TEST(test_handle_param_dispatches_nft);
    RUN_TEST(test_handle_param_dispatches_datetime);
    RUN_TEST(test_handle_param_dispatches_duration);
    RUN_TEST(test_handle_param_dispatches_unit);
    RUN_TEST(test_handle_param_dispatches_enum);
    RUN_TEST(test_handle_param_dispatches_trusted_name);
    RUN_TEST(test_handle_param_dispatches_calldata);
    RUN_TEST(test_handle_param_dispatches_token);
    RUN_TEST(test_handle_param_dispatches_network);
    RUN_TEST(test_handle_param_dispatches_group);
    RUN_TEST(test_format_field_raw);
    RUN_TEST(test_format_field_amount);
    RUN_TEST(test_format_field_token_amount);
    RUN_TEST(test_format_field_nft);
    RUN_TEST(test_format_field_datetime);
    RUN_TEST(test_format_field_duration);
    RUN_TEST(test_format_field_unit);
    RUN_TEST(test_format_field_enum);
    RUN_TEST(test_format_field_trusted_name);
    RUN_TEST(test_format_field_calldata);
    RUN_TEST(test_format_field_token);
    RUN_TEST(test_format_field_network);
    RUN_TEST(test_format_field_group);
    RUN_TEST(test_format_field_unsupported_type_rejected);
    RUN_TEST(test_cleanup_field_null_safe);
    RUN_TEST(test_cleanup_field_with_group_routes_through_group_cleanup);
    RUN_TEST(test_constraint_with_always_visibility_single_call_rejected);
    RUN_TEST(test_constraint_value_above_uint8_max_rejected);
    RUN_TEST(test_handle_separator_populates_field);
    return UNITY_END();
}
