/**
 * @file test_path_slice.c
 * @brief Unit tests for the slice-path TLV parser at
 *        src/features/generic_tx_parser/gtp_path_slice.c.
 *
 * Slice paths cap a dynamic-array path to a [start, end) sub-range. The
 * TLV carries:
 *   - TAG_START (0x01): inclusive start index, 2 bytes minimum,
 *                       marked present via has_start,
 *   - TAG_END   (0x02): exclusive end index, 2 bytes minimum,
 *                       marked present via has_end.
 * Both tags are optional and ENFORCE_UNIQUE_TAG; either end can be
 * omitted to mean "unbounded on that side".
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_path_slice.h"

static bool run_tlv(const uint8_t *bytes, size_t size, s_slice_args *args) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_path_slice_context ctx = {.args = args};
    return handle_slice_struct(&buf, &ctx);
}

void test_tlv_start_end_populated(void) {
    const uint8_t bytes[] = {
        0x01,
        0x02,
        0x00,
        0x05,  // START = 5
        0x02,
        0x02,
        0x00,
        0x0A,  // END   = 10
    };
    s_slice_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_TRUE(args.has_start);
    TEST_ASSERT_EQUAL(args.start, 5);
    TEST_ASSERT_TRUE(args.has_end);
    TEST_ASSERT_EQUAL(args.end, 10);
}

void test_tlv_only_start(void) {
    const uint8_t bytes[] = {0x01, 0x02, 0x00, 0x03};
    s_slice_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_TRUE(args.has_start);
    TEST_ASSERT_EQUAL(args.start, 3);
    TEST_ASSERT_FALSE(args.has_end);
}

void test_tlv_only_end(void) {
    const uint8_t bytes[] = {0x02, 0x02, 0x00, 0x07};
    s_slice_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_FALSE(args.has_start);
    TEST_ASSERT_TRUE(args.has_end);
    TEST_ASSERT_EQUAL(args.end, 7);
}

void test_tlv_empty_buffer_accepted(void) {
    s_slice_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(NULL, 0, &args));
    TEST_ASSERT_FALSE(args.has_start);
    TEST_ASSERT_FALSE(args.has_end);
}

void test_tlv_start_end_read_big_endian(void) {
    const uint8_t bytes[] = {
        0x01,
        0x02,
        0x12,
        0x34,
        0x02,
        0x02,
        0xAB,
        0xCD,
    };
    s_slice_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_EQUAL(args.start, 0x1234);
    // 0xABCD interpreted as signed int16 = -21555
    TEST_ASSERT_EQUAL(args.end, (int16_t) 0xABCD);
}

void test_tlv_start_one_byte_payload_rejected(void) {
    const uint8_t bytes[] = {0x01, 0x01, 0xAA};
    s_slice_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_FALSE(args.has_start);
}

void test_tlv_end_one_byte_payload_rejected(void) {
    const uint8_t bytes[] = {0x02, 0x01, 0xBB};
    s_slice_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_FALSE(args.has_end);
}

void test_tlv_duplicate_start_rejected(void) {
    const uint8_t bytes[] = {
        0x01,
        0x02,
        0x00,
        0x01,
        0x01,
        0x02,
        0x00,
        0x02,
    };
    s_slice_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
}

void test_tlv_duplicate_end_rejected(void) {
    const uint8_t bytes[] = {
        0x02,
        0x02,
        0x00,
        0x01,
        0x02,
        0x02,
        0x00,
        0x02,
    };
    s_slice_args args = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &args));
}

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tlv_start_end_populated);
    RUN_TEST(test_tlv_only_start);
    RUN_TEST(test_tlv_only_end);
    RUN_TEST(test_tlv_empty_buffer_accepted);
    RUN_TEST(test_tlv_start_end_read_big_endian);
    RUN_TEST(test_tlv_start_one_byte_payload_rejected);
    RUN_TEST(test_tlv_end_one_byte_payload_rejected);
    RUN_TEST(test_tlv_duplicate_start_rejected);
    RUN_TEST(test_tlv_duplicate_end_rejected);
    return UNITY_END();
}
