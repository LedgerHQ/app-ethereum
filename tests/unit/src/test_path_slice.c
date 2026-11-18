/**
 * @file test_path_slice.c
 * @brief Unit tests for the slice-path TLV parser and the bounds it gives, at
 *        src/features/generic_tx_parser/gtp_path_slice.c.
 *
 * Slice paths cap a dynamic-array path to a [start, end) sub-range. The
 * TLV carries:
 *   - TAG_START (0x01): inclusive start index, 1 or 2 bytes,
 *                       marked present via has_start,
 *   - TAG_END   (0x02): exclusive end index, 1 or 2 bytes,
 *                       marked present via has_end.
 * One byte is 0 to 255 and two bytes are two's complement, any other
 * length is refused. Both tags are optional and ENFORCE_UNIQUE_TAG;
 * either end can be omitted to mean "unbounded on that side".
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

// The length gives the sign: one byte is 0 to 255, two bytes are two's complement
void test_tlv_one_byte_bounds_are_not_negative(void) {
    const uint8_t bytes[] = {0x01, 0x01, 0xAA, 0x02, 0x01, 0xFF};
    s_slice_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_TRUE(args.has_start);
    TEST_ASSERT_EQUAL(170, args.start);
    TEST_ASSERT_TRUE(args.has_end);
    TEST_ASSERT_EQUAL(255, args.end);
}

void test_tlv_one_byte_and_two_byte_bounds_of_the_same_value_are_equal(void) {
    const uint8_t one_byte[] = {0x01, 0x01, 0x05, 0x02, 0x01, 0xFF};
    const uint8_t two_bytes[] = {0x01, 0x02, 0x00, 0x05, 0x02, 0x02, 0x00, 0xFF};
    s_slice_args one = {0};
    s_slice_args two = {0};

    TEST_ASSERT_TRUE(run_tlv(one_byte, sizeof(one_byte), &one));
    TEST_ASSERT_TRUE(run_tlv(two_bytes, sizeof(two_bytes), &two));
    TEST_ASSERT_EQUAL(two.start, one.start);
    TEST_ASSERT_EQUAL(two.end, one.end);
}

void test_tlv_two_byte_bounds_are_two_s_complement(void) {
    const uint8_t bytes[] = {0x01, 0x02, 0xFF, 0xFF, 0x02, 0x02, 0x80, 0x00};
    s_slice_args args = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &args));
    TEST_ASSERT_EQUAL(-1, args.start);
    TEST_ASSERT_EQUAL(-32768, args.end);
}

void test_tlv_bounds_of_another_length_rejected(void) {
    const uint8_t start_empty[] = {0x01, 0x00};
    const uint8_t start_three[] = {0x01, 0x03, 0x00, 0x01, 0xFF};
    const uint8_t start_four[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x02};
    const uint8_t end_empty[] = {0x02, 0x00};
    const uint8_t end_three[] = {0x02, 0x03, 0x00, 0x01, 0xFF};
    const uint8_t end_four[] = {0x02, 0x04, 0xFF, 0xFF, 0xFF, 0xFE};
    s_slice_args args = {0};

    TEST_ASSERT_FALSE(run_tlv(start_empty, sizeof(start_empty), &args));
    TEST_ASSERT_FALSE(run_tlv(start_three, sizeof(start_three), &args));
    TEST_ASSERT_FALSE(run_tlv(start_four, sizeof(start_four), &args));
    TEST_ASSERT_FALSE(run_tlv(end_empty, sizeof(end_empty), &args));
    TEST_ASSERT_FALSE(run_tlv(end_three, sizeof(end_three), &args));
    TEST_ASSERT_FALSE(run_tlv(end_four, sizeof(end_four), &args));
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

// =============================================================================
// slice_resolve
// =============================================================================

void test_resolve_absent_bounds_are_the_whole_length(void) {
    const s_slice_args args = {0};
    const s_slice_bounds bounds = slice_resolve(&args, 10);

    TEST_ASSERT_EQUAL(0, bounds.start);
    TEST_ASSERT_EQUAL(10, bounds.end);
}

void test_resolve_negative_bounds_count_back_from_the_end(void) {
    const s_slice_args args = {.has_start = true, .start = -3, .has_end = true, .end = -1};
    const s_slice_bounds bounds = slice_resolve(&args, 10);

    TEST_ASSERT_EQUAL(7, bounds.start);
    TEST_ASSERT_EQUAL(9, bounds.end);
}

// the bounds are not checked: a bound that reaches outside of the length stays so
void test_resolve_does_not_check_the_bounds(void) {
    const s_slice_args before = {.has_start = true, .start = -20};
    const s_slice_args past = {.has_start = true, .start = 20, .has_end = true, .end = 30};
    const s_slice_args inverted = {.has_start = true, .start = 5, .has_end = true, .end = 2};

    TEST_ASSERT_EQUAL(-10, slice_resolve(&before, 10).start);
    TEST_ASSERT_EQUAL(20, slice_resolve(&past, 10).start);
    TEST_ASSERT_EQUAL(30, slice_resolve(&past, 10).end);
    TEST_ASSERT_EQUAL(5, slice_resolve(&inverted, 10).start);
    TEST_ASSERT_EQUAL(2, slice_resolve(&inverted, 10).end);
}

// =============================================================================
// slice_bytes
// =============================================================================

static bool bytes_of(bool has_start, int16_t start, bool has_end, int16_t end, uint16_t length) {
    const s_slice_args args = {.has_start = has_start,
                               .start = start,
                               .has_end = has_end,
                               .end = end};
    uint16_t offset = 0xFFFF;
    uint16_t size = 0xFFFF;

    return slice_bytes(&args, length, &offset, &size);
}

void test_bytes_of_the_whole_value(void) {
    const s_slice_args args = {0};
    uint16_t offset = 0xFFFF;
    uint16_t size = 0xFFFF;

    TEST_ASSERT_TRUE(slice_bytes(&args, 10, &offset, &size));
    TEST_ASSERT_EQUAL(0, offset);
    TEST_ASSERT_EQUAL(10, size);
}

void test_bytes_of_a_range(void) {
    const s_slice_args args = {.has_start = true, .start = 2, .has_end = true, .end = 6};
    uint16_t offset = 0;
    uint16_t size = 0;

    TEST_ASSERT_TRUE(slice_bytes(&args, 10, &offset, &size));
    TEST_ASSERT_EQUAL(2, offset);
    TEST_ASSERT_EQUAL(4, size);
}

void test_bytes_of_the_last_ones(void) {
    const s_slice_args args = {.has_start = true, .start = -4};
    uint16_t offset = 0;
    uint16_t size = 0;

    TEST_ASSERT_TRUE(slice_bytes(&args, 10, &offset, &size));
    TEST_ASSERT_EQUAL(6, offset);
    TEST_ASSERT_EQUAL(4, size);
}

void test_bytes_refused_when_nothing_is_kept(void) {
    TEST_ASSERT_FALSE(bytes_of(true, 4, true, 4, 10));
    TEST_ASSERT_FALSE(bytes_of(true, 6, true, 2, 10));
    TEST_ASSERT_FALSE(bytes_of(true, 0, true, 0, 10));
    // and a value that is empty has nothing to keep
    TEST_ASSERT_FALSE(bytes_of(false, 0, false, 0, 0));
}

void test_bytes_refused_when_outside_of_the_value(void) {
    TEST_ASSERT_FALSE(bytes_of(true, 0, true, 11, 10));
    TEST_ASSERT_FALSE(bytes_of(true, 10, false, 0, 10));
    TEST_ASSERT_FALSE(bytes_of(true, 20, false, 0, 10));
    // a bound that counts back past the start of the value
    TEST_ASSERT_FALSE(bytes_of(true, -11, false, 0, 10));
    TEST_ASSERT_FALSE(bytes_of(false, 0, true, -11, 10));
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
    RUN_TEST(test_tlv_one_byte_bounds_are_not_negative);
    RUN_TEST(test_tlv_one_byte_and_two_byte_bounds_of_the_same_value_are_equal);
    RUN_TEST(test_tlv_two_byte_bounds_are_two_s_complement);
    RUN_TEST(test_tlv_bounds_of_another_length_rejected);
    RUN_TEST(test_tlv_duplicate_start_rejected);
    RUN_TEST(test_tlv_duplicate_end_rejected);
    RUN_TEST(test_resolve_absent_bounds_are_the_whole_length);
    RUN_TEST(test_resolve_negative_bounds_count_back_from_the_end);
    RUN_TEST(test_resolve_does_not_check_the_bounds);
    RUN_TEST(test_bytes_of_the_whole_value);
    RUN_TEST(test_bytes_of_a_range);
    RUN_TEST(test_bytes_of_the_last_ones);
    RUN_TEST(test_bytes_refused_when_nothing_is_kept);
    RUN_TEST(test_bytes_refused_when_outside_of_the_value);
    return UNITY_END();
}
