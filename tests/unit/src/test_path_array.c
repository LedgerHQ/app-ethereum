/**
 * @file test_path_array.c
 * @brief Unit tests for the array-path TLV parser at
 *        src/features/generic_tx_parser/gtp_path_array.c.
 *
 * Array paths describe how to index into an ABI-encoded array when the
 * field referenced by a GCS parameter lives behind a dynamic-array
 * indirection. The TLV carries:
 *   - TAG_WEIGHT  (0x01): per-element size, 1 byte minimum,
 *   - TAG_START   (0x02): inclusive start index, 2 bytes minimum,
 *                          marked present via has_start,
 *   - TAG_END     (0x03): exclusive end index, 2 bytes minimum,
 *                          marked present via has_end.
 *
 * Each handler enforces a strict minimum payload size (`size <
 * sizeof(field)` is rejected) and the framework enforces tag
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

void test_tlv_extra_weight_bytes_keeps_first_byte(void) {
    // The handler only reads ptr[0] for weight, so passing 4 bytes
    // succeeds and the first byte wins.
    const uint8_t bytes[] = {0x01, 0x04, 0x33, 0xAA, 0xBB, 0xCC};
    s_array_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_EQUAL(args.weight, 0x33);
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

void test_tlv_start_one_byte_payload_rejected(void) {
    const uint8_t bytes[] = {0x02, 0x01, 0xAA};  // START needs 2 bytes
    s_array_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_FALSE(args.has_start);  // never marked present
}

void test_tlv_end_one_byte_payload_rejected(void) {
    const uint8_t bytes[] = {0x03, 0x01, 0xBB};
    s_array_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_FALSE(args.has_end);
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
    RUN_TEST(test_tlv_extra_weight_bytes_keeps_first_byte);
    RUN_TEST(test_tlv_start_end_read_big_endian);
    RUN_TEST(test_tlv_weight_empty_payload_rejected);
    RUN_TEST(test_tlv_weight_zero_rejected);
    RUN_TEST(test_tlv_start_one_byte_payload_rejected);
    RUN_TEST(test_tlv_end_one_byte_payload_rejected);
    RUN_TEST(test_tlv_duplicate_weight_rejected);
    RUN_TEST(test_tlv_duplicate_start_rejected);
    return UNITY_END();
}
