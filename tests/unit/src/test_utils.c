/**
 * @file test_utils.c
 * @brief Unit tests for the app-level string / buffer helpers in src/utils.c.
 *
 * utils.c is tiny but every helper is on a path that touches user-visible
 * data:
 *   - format_signed_int_be is what the GCS / EIP-712 layers call to render
 *     int8/16/.../256 fields. The signed branch picks between int64,
 *     uint128, and uint256 formatters based on type_size, so each width
 *     gets a dedicated case.
 *   - str_cpy_explicit_trunc decides whether a string is shown intact or
 *     truncated with "..." — the truncation marker placement is what makes
 *     the helper non-trivial.
 *   - buf_shrink_expand is used to right-align BE values into fixed-width
 *     slots; getting the zero-pad direction wrong silently corrupts amounts.
 *   - reverseString underpins every tostringNNN call.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "utils.h"

// =============================================================================
// reverseString
// =============================================================================

void test_reverseString_even_length(void) {
    char s[] = "abcdef";
    reverseString(s, 6);
    TEST_ASSERT_EQUAL_STRING(s, "fedcba");
}

void test_reverseString_odd_length(void) {
    char s[] = "12345";
    reverseString(s, 5);
    TEST_ASSERT_EQUAL_STRING(s, "54321");
}

void test_reverseString_partial_length(void) {
    // length argument is what gets reversed; the rest of the string is left alone.
    char s[] = "ABCDxyz";
    reverseString(s, 4);
    TEST_ASSERT_EQUAL_STRING(s, "DCBAxyz");
}

void test_reverseString_single_char(void) {
    char s[] = "Q";
    reverseString(s, 1);
    TEST_ASSERT_EQUAL_STRING(s, "Q");
}

// =============================================================================
// buf_shrink_expand
// =============================================================================

void test_buf_shrink_expand_expand_left_zero_pads(void) {
    const uint8_t src[] = {0xAA, 0xBB, 0xCC};
    uint8_t dst[8];
    memset(dst, 0xFF, sizeof(dst));
    buf_shrink_expand(src, sizeof(src), dst, sizeof(dst));
    const uint8_t expected[] = {0, 0, 0, 0, 0, 0xAA, 0xBB, 0xCC};
    TEST_ASSERT_EQUAL_MEMORY(dst, expected, sizeof(dst));
}

void test_buf_shrink_expand_shrink_drops_msb(void) {
    const uint8_t src[] = {0x11, 0x22, 0x33, 0xAA, 0xBB, 0xCC};
    uint8_t dst[3];
    buf_shrink_expand(src, sizeof(src), dst, sizeof(dst));
    const uint8_t expected[] = {0xAA, 0xBB, 0xCC};
    TEST_ASSERT_EQUAL_MEMORY(dst, expected, sizeof(dst));
}

void test_buf_shrink_expand_equal_size(void) {
    const uint8_t src[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t dst[4];
    memset(dst, 0xFF, sizeof(dst));
    buf_shrink_expand(src, sizeof(src), dst, sizeof(dst));
    TEST_ASSERT_EQUAL_MEMORY(dst, src, sizeof(dst));
}

// =============================================================================
// str_cpy_explicit_trunc
// =============================================================================

void test_str_cpy_trunc_fits(void) {
    char dst[16] = {0};
    str_cpy_explicit_trunc("hello", 5, dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING(dst, "hello");
}

void test_str_cpy_trunc_appends_ellipsis(void) {
    // "Hello, world" (12 bytes) won't fit in a 9-byte buffer (8 chars + NUL):
    // we expect 5 source chars + "...\0" = 9 bytes total.
    char dst[9];
    memset(dst, 'X', sizeof(dst));
    str_cpy_explicit_trunc("Hello, world", 12, dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING(dst, "Hello...");
}

void test_str_cpy_trunc_exact_marker_size_empties(void) {
    // dst_size == sizeof("...") (== 4 bytes) cannot represent any truncation
    // because there'd be no room for even a single source character before
    // "...", so the helper just NUL-terminates.
    char dst[4];
    memset(dst, 'X', sizeof(dst));
    str_cpy_explicit_trunc("Hello", 5, dst, sizeof(dst));
    TEST_ASSERT_EQUAL(dst[0], '\0');
}

void test_str_cpy_trunc_zero_dst_is_noop(void) {
    char canary = 'Z';
    str_cpy_explicit_trunc("Hello", 5, &canary, 0);
    TEST_ASSERT_EQUAL(canary, 'Z');
}

void test_str_cpy_trunc_exact_fit_terminates(void) {
    // dst is exactly src + NUL (5 + 1 = 6 bytes). Should fit without "...".
    char dst[6];
    memset(dst, 'X', sizeof(dst));
    str_cpy_explicit_trunc("Hello", 5, dst, sizeof(dst));
    TEST_ASSERT_EQUAL_STRING(dst, "Hello");
}

// =============================================================================
// format_signed_int_be
// =============================================================================
//
// Branch matrix:
//                          length=0  length>type   MSB clear   MSB set
//   any                    false     false         unsigned    signed
// And the signed branch dispatches on type_size:
//   type_size <= 8         → int64 (format_i64)
//   type_size <= 16        → int128 (tostring128_signed)
//   type_size <= 32        → int256 (tostring256_signed)
//   type_size  > 32        → false

void test_format_signed_int_be_length_zero_rejected(void) {
    uint8_t data[] = {0x42};
    char out[40];
    TEST_ASSERT_FALSE(format_signed_int_be(data, 0, 32, out, sizeof(out)));
}

void test_format_signed_int_be_length_exceeds_type_rejected(void) {
    uint8_t data[] = {0, 0, 0, 0, 0, 0, 0, 0, 0xFF};  // 9 bytes
    char out[40];
    TEST_ASSERT_FALSE(format_signed_int_be(data, 9, 8, out, sizeof(out)));
}

void test_format_signed_int_be_length_above_uint8_rejected(void) {
    // A dynamic calldata leaf carries its length in a host-chosen word. 260 is
    // 0x0104: narrowed to a uint8_t it reads as 4, which would format the first four
    // bytes of a 260-byte value and report success.
    uint8_t data[260] = {0xDE, 0xAD, 0xBE, 0xEF};
    char out[96];
    TEST_ASSERT_FALSE(format_signed_int_be(data, sizeof(data), 32, out, sizeof(out)));
}

void test_format_signed_int_be_length_multiple_of_256_rejected(void) {
    // 256 narrows to 0, which the length==0 guard already caught; kept so both sides
    // of the wrap are pinned.
    uint8_t data[256] = {0x01};
    char out[96];
    TEST_ASSERT_FALSE(format_signed_int_be(data, sizeof(data), 32, out, sizeof(out)));
}

void test_format_signed_int_be_short_value_treated_unsigned(void) {
    // length < type_size → always unsigned, regardless of MSB
    uint8_t data[] = {0xFF};
    char out[40] = {0};
    TEST_ASSERT_TRUE(format_signed_int_be(data, 1, 4, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "255");
}

void test_format_signed_int_be_full_width_msb_clear_unsigned(void) {
    // int8 = 0x7F → +127
    uint8_t data[] = {0x7F};
    char out[40] = {0};
    TEST_ASSERT_TRUE(format_signed_int_be(data, 1, 1, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "127");
}

void test_format_signed_int_be_int8_negative(void) {
    // int8 = 0xFF → -1
    uint8_t data[] = {0xFF};
    char out[40] = {0};
    TEST_ASSERT_TRUE(format_signed_int_be(data, 1, 1, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "-1");
}

void test_format_signed_int_be_int64_negative(void) {
    // int64 = 0xFFFFFFFFFFFFFFFF → -1
    uint8_t data[8];
    memset(data, 0xFF, sizeof(data));
    char out[40] = {0};
    TEST_ASSERT_TRUE(format_signed_int_be(data, 8, 8, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "-1");
}

void test_format_signed_int_be_int64_min(void) {
    // int64 min = 0x80...00
    uint8_t data[8] = {0x80, 0, 0, 0, 0, 0, 0, 0};
    char out[40] = {0};
    TEST_ASSERT_TRUE(format_signed_int_be(data, 8, 8, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "-9223372036854775808");
}

void test_format_signed_int_be_int128_negative(void) {
    // int128 = all 0xFF → -1
    uint8_t data[16];
    memset(data, 0xFF, sizeof(data));
    char out[60] = {0};
    TEST_ASSERT_TRUE(format_signed_int_be(data, 16, 16, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "-1");
}

void test_format_signed_int_be_int256_negative(void) {
    // int256 = all 0xFF → -1
    uint8_t data[32];
    memset(data, 0xFF, sizeof(data));
    char out[100] = {0};
    TEST_ASSERT_TRUE(format_signed_int_be(data, 32, 32, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "-1");
}

void test_format_signed_int_be_oversize_type_rejected(void) {
    // type_size > 32 falls through every branch and returns false.
    uint8_t data[40];
    memset(data, 0xFF, sizeof(data));
    char out[100] = {0};
    TEST_ASSERT_FALSE(format_signed_int_be(data, 33, 33, out, sizeof(out)));
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
    RUN_TEST(test_reverseString_even_length);
    RUN_TEST(test_reverseString_odd_length);
    RUN_TEST(test_reverseString_partial_length);
    RUN_TEST(test_reverseString_single_char);
    RUN_TEST(test_buf_shrink_expand_expand_left_zero_pads);
    RUN_TEST(test_buf_shrink_expand_shrink_drops_msb);
    RUN_TEST(test_buf_shrink_expand_equal_size);
    RUN_TEST(test_str_cpy_trunc_fits);
    RUN_TEST(test_str_cpy_trunc_appends_ellipsis);
    RUN_TEST(test_str_cpy_trunc_exact_marker_size_empties);
    RUN_TEST(test_str_cpy_trunc_zero_dst_is_noop);
    RUN_TEST(test_str_cpy_trunc_exact_fit_terminates);
    RUN_TEST(test_format_signed_int_be_length_zero_rejected);
    RUN_TEST(test_format_signed_int_be_length_exceeds_type_rejected);
    RUN_TEST(test_format_signed_int_be_length_above_uint8_rejected);
    RUN_TEST(test_format_signed_int_be_length_multiple_of_256_rejected);
    RUN_TEST(test_format_signed_int_be_short_value_treated_unsigned);
    RUN_TEST(test_format_signed_int_be_full_width_msb_clear_unsigned);
    RUN_TEST(test_format_signed_int_be_int8_negative);
    RUN_TEST(test_format_signed_int_be_int64_negative);
    RUN_TEST(test_format_signed_int_be_int64_min);
    RUN_TEST(test_format_signed_int_be_int128_negative);
    RUN_TEST(test_format_signed_int_be_int256_negative);
    RUN_TEST(test_format_signed_int_be_oversize_type_rejected);
    return UNITY_END();
}
