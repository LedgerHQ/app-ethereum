/**
 * @file test_path_array.c
 * @brief Unit tests for the array-path TLV parser at
 *        src/features/generic_tx_parser/gtp_path_array.c.
 *
 * Array paths describe how to index into an ABI-encoded array when the
 * field referenced by a GCS parameter lives behind a dynamic-array
 * indirection. The TLV carries:
 *   - TAG_WEIGHT  (0x01): per-element size, 1 to 255 (read as a whole uint8),
 *   - TAG_START   (0x02): inclusive start index, 1 or 2 bytes,
 *                          marked present via has_start,
 *   - TAG_END     (0x03): exclusive end index, 1 or 2 bytes,
 *                          marked present via has_end.
 *
 * One byte is 0 to 255 and two bytes are two's complement, any other
 * length is refused, and the framework enforces tag
 * uniqueness. WEIGHT is the only mandatory-shaped tag; the caller may
 * leave START / END absent to mean "whole array", which is conveyed
 * via the has_start / has_end booleans.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_path_array.h"

// =============================================================================
// Helpers
// =============================================================================

static bool run_tlv(const uint8_t *bytes, size_t size, s_array_args *args) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_path_array_context ctx = {.args = args};
    return handle_array_struct(&buf, &ctx);
}

// =============================================================================
// Happy paths
// =============================================================================

void test_tlv_weight_start_end_populated(void) {
    const uint8_t bytes[] = {
        0x01,
        0x01,
        0x20,  // WEIGHT  = 0x20
        0x02,
        0x02,
        0x00,
        0x05,  // START   = 5  (BE u16)
        0x03,
        0x02,
        0x00,
        0x0A,  // END     = 10
    };
    s_array_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_EQUAL(args.weight, 0x20);
    TEST_ASSERT_TRUE(args.has_start);
    TEST_ASSERT_EQUAL(args.start, 5);
    TEST_ASSERT_TRUE(args.has_end);
    TEST_ASSERT_EQUAL(args.end, 10);
}

void test_tlv_only_weight_keeps_has_start_end_false(void) {
    const uint8_t bytes[] = {0x01, 0x01, 0x42};
    s_array_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_EQUAL(args.weight, 0x42);
    TEST_ASSERT_FALSE(args.has_start);
    TEST_ASSERT_FALSE(args.has_end);
}

void test_tlv_empty_buffer_accepted(void) {
    // The parser does not require any specific tag presence — an empty
    // payload leaves the args at their pre-init values (zero / false).
    s_array_args args = {0};
    args.weight = 0xCC;  // Detectable canary
    args.has_start = false;
    TEST_ASSERT_TRUE(run_tlv(NULL, 0, &args));
    // No tag handler ran, so the canary remains.
    TEST_ASSERT_EQUAL(args.weight, 0xCC);
    TEST_ASSERT_FALSE(args.has_start);
    TEST_ASSERT_FALSE(args.has_end);
}

// The weight is read as every uint8 of a descriptor: the whole payload, so a leading zero is
// harmless and a value above 255 is refused. It is not the first byte of a longer payload
void test_tlv_longer_weight_is_read_whole(void) {
    const uint8_t padded[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x33};
    const uint8_t two_bytes[] = {0x01, 0x02, 0x00, 0x20};
    const uint8_t above_255[] = {0x01, 0x02, 0x01, 0x00};
    const uint8_t first_byte_only[] = {0x01, 0x04, 0x33, 0xAA, 0xBB, 0xCC};
    const uint8_t zero_padded[] = {0x01, 0x02, 0x00, 0x00};
    s_array_args args = {0};

    TEST_ASSERT_TRUE(run_tlv(padded, sizeof(padded), &args));
    TEST_ASSERT_EQUAL(0x33, args.weight);
    TEST_ASSERT_TRUE(run_tlv(two_bytes, sizeof(two_bytes), &args));
    TEST_ASSERT_EQUAL(0x20, args.weight);
    TEST_ASSERT_FALSE(run_tlv(above_255, sizeof(above_255), &args));
    TEST_ASSERT_FALSE(run_tlv(first_byte_only, sizeof(first_byte_only), &args));
    TEST_ASSERT_FALSE(run_tlv(zero_padded, sizeof(zero_padded), &args));
}

void test_tlv_start_end_read_big_endian(void) {
    const uint8_t bytes[] = {
        0x02,
        0x02,
        0x12,
        0x34,  // START = 0x1234
        0x03,
        0x02,
        0xAB,
        0xCD,  // END   = 0xABCD (stored as int16 → -21555 after cast)
    };
    s_array_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_TRUE(args.has_start);
    TEST_ASSERT_EQUAL(args.start, 0x1234);
    TEST_ASSERT_TRUE(args.has_end);
    // 0xABCD interpreted as signed int16 = -21555
    TEST_ASSERT_EQUAL(args.end, (int16_t) 0xABCD);
}

// =============================================================================
// Rejections — short payload
// =============================================================================

void test_tlv_weight_empty_payload_rejected(void) {
    const uint8_t bytes[] = {0x01, 0x00};  // WEIGHT with size=0
    s_array_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
}

void test_tlv_weight_zero_rejected(void) {
    const uint8_t bytes[] = {0x01, 0x01, 0x00};  // WEIGHT = 0
    s_array_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
}

// The length gives the sign: one byte is 0 to 255, two bytes are two's complement
void test_tlv_one_byte_bounds_are_not_negative(void) {
    const uint8_t bytes[] = {0x02, 0x01, 0xAA, 0x03, 0x01, 0xFF};
    s_array_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_TRUE(args.has_start);
    TEST_ASSERT_EQUAL(170, args.start);
    TEST_ASSERT_TRUE(args.has_end);
    TEST_ASSERT_EQUAL(255, args.end);
}

void test_tlv_one_byte_and_two_byte_bounds_of_the_same_value_are_equal(void) {
    const uint8_t one_byte[] = {0x02, 0x01, 0x05, 0x03, 0x01, 0xFF};
    const uint8_t two_bytes[] = {0x02, 0x02, 0x00, 0x05, 0x03, 0x02, 0x00, 0xFF};
    s_array_args one = {0};
    s_array_args two = {0};

    TEST_ASSERT_TRUE(run_tlv(one_byte, sizeof(one_byte), &one));
    TEST_ASSERT_TRUE(run_tlv(two_bytes, sizeof(two_bytes), &two));
    TEST_ASSERT_EQUAL(two.start, one.start);
    TEST_ASSERT_EQUAL(two.end, one.end);
}

void test_tlv_two_byte_bounds_are_two_s_complement(void) {
    const uint8_t bytes[] = {0x02, 0x02, 0xFF, 0xFF, 0x03, 0x02, 0x80, 0x00};
    s_array_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_EQUAL(-1, args.start);
    TEST_ASSERT_EQUAL(-32768, args.end);
}

void test_tlv_bounds_of_another_length_rejected(void) {
    const uint8_t start_empty[] = {0x02, 0x00};
    const uint8_t start_three[] = {0x02, 0x03, 0x00, 0x01, 0xFF};
    const uint8_t start_four[] = {0x02, 0x04, 0x00, 0x00, 0x00, 0x02};
    const uint8_t end_empty[] = {0x03, 0x00};
    const uint8_t end_three[] = {0x03, 0x03, 0x00, 0x01, 0xFF};
    const uint8_t end_four[] = {0x03, 0x04, 0xFF, 0xFF, 0xFF, 0xFE};
    s_array_args args = {0};

    TEST_ASSERT_FALSE(run_tlv(start_empty, sizeof(start_empty), &args));
    TEST_ASSERT_FALSE(run_tlv(start_three, sizeof(start_three), &args));
    TEST_ASSERT_FALSE(run_tlv(start_four, sizeof(start_four), &args));
    TEST_ASSERT_FALSE(run_tlv(end_empty, sizeof(end_empty), &args));
    TEST_ASSERT_FALSE(run_tlv(end_three, sizeof(end_three), &args));
    TEST_ASSERT_FALSE(run_tlv(end_four, sizeof(end_four), &args));
}

// =============================================================================
// Rejections — tag uniqueness
// =============================================================================

void test_tlv_duplicate_weight_rejected(void) {
    const uint8_t bytes[] = {
        0x01,
        0x01,
        0x10,
        0x01,
        0x01,
        0x20,  // duplicate WEIGHT
    };
    s_array_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
}

void test_tlv_duplicate_start_rejected(void) {
    const uint8_t bytes[] = {
        0x02,
        0x02,
        0x00,
        0x05,
        0x02,
        0x02,
        0x00,
        0x06,
    };
    s_array_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tlv_weight_start_end_populated);
    RUN_TEST(test_tlv_only_weight_keeps_has_start_end_false);
    RUN_TEST(test_tlv_empty_buffer_accepted);
    RUN_TEST(test_tlv_longer_weight_is_read_whole);
    RUN_TEST(test_tlv_start_end_read_big_endian);
    RUN_TEST(test_tlv_weight_empty_payload_rejected);
    RUN_TEST(test_tlv_weight_zero_rejected);
    RUN_TEST(test_tlv_one_byte_bounds_are_not_negative);
    RUN_TEST(test_tlv_one_byte_and_two_byte_bounds_of_the_same_value_are_equal);
    RUN_TEST(test_tlv_two_byte_bounds_are_two_s_complement);
    RUN_TEST(test_tlv_bounds_of_another_length_rejected);
    RUN_TEST(test_tlv_duplicate_weight_rejected);
    RUN_TEST(test_tlv_duplicate_start_rejected);
    return UNITY_END();
}
