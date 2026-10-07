/**
 * @file test_uint128.c
 * @brief Unit tests for uint128_t arithmetic helpers in src/uint128.c.
 *
 * uint128_t is the building block for uint256_t and for every amount the
 * device formats on screen, so the boundary behavior of the shift / arith /
 * format helpers must hold exactly. Tests are organised one CMocka case per
 * public function with a focus on:
 *   - the shift boundaries (0, 1, 63, 64, 65, 127, 128, > 128) where 64-bit
 *     halves are split or swapped,
 *   - carry / borrow propagation in add128 / sub128,
 *   - mul128 against precomputed full-width products,
 *   - tostring128 base validation and buffer-overrun protection,
 *   - convertUint*BE input-length guards.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "uint128.h"
#include "uint_common.h"

// =============================================================================
// Constructors / accessors
// =============================================================================

void test_clear128_zeros_both_halves(void) {
    uint128_t n;
    UPPER(n) = 0xDEADBEEFDEADBEEFULL;
    LOWER(n) = 0xCAFEBABECAFEBABEULL;

    clear128(&n);
    TEST_ASSERT_EQUAL(UPPER(n), 0);
    TEST_ASSERT_EQUAL(LOWER(n), 0);
}

void test_zero128_detects_zero_and_nonzero(void) {
    uint128_t z = {{0, 0}};
    uint128_t low_only = {{0, 1}};
    uint128_t high_only = {{1, 0}};
    TEST_ASSERT_TRUE(zero128(&z));
    TEST_ASSERT_FALSE(zero128(&low_only));
    TEST_ASSERT_FALSE(zero128(&high_only));
}

void test_copy128_preserves_both_halves(void) {
    uint128_t src = {{0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL}};
    uint128_t dst = {{0, 0}};
    copy128(&dst, &src);
    TEST_ASSERT_EQUAL(UPPER(dst), UPPER(src));
    TEST_ASSERT_EQUAL(LOWER(dst), LOWER(src));
}

void test_readu128BE_reads_big_endian(void) {
    // Upper = 0x0123456789ABCDEF, Lower = 0xFEDCBA9876543210
    uint8_t buf[16] = {0x01,
                       0x23,
                       0x45,
                       0x67,
                       0x89,
                       0xAB,
                       0xCD,
                       0xEF,
                       0xFE,
                       0xDC,
                       0xBA,
                       0x98,
                       0x76,
                       0x54,
                       0x32,
                       0x10};
    uint128_t n;
    readu128BE(buf, &n);
    TEST_ASSERT_EQUAL(UPPER(n), 0x0123456789ABCDEFULL);
    TEST_ASSERT_EQUAL(LOWER(n), 0xFEDCBA9876543210ULL);
}

// =============================================================================
// Comparison
// =============================================================================

void test_equal128(void) {
    uint128_t a = {{1, 2}};
    uint128_t b = {{1, 2}};
    uint128_t c = {{1, 3}};
    uint128_t d = {{2, 2}};
    TEST_ASSERT_TRUE(equal128(&a, &b));
    TEST_ASSERT_FALSE(equal128(&a, &c));
    TEST_ASSERT_FALSE(equal128(&a, &d));
}

void test_gt128_and_gte128(void) {
    // High word dominates
    uint128_t big = {{2, 0}};
    uint128_t small_hi = {{1, 0xFFFFFFFFFFFFFFFFULL}};
    TEST_ASSERT_TRUE(gt128(&big, &small_hi));
    TEST_ASSERT_FALSE(gt128(&small_hi, &big));

    // Tie on high → compare low
    uint128_t a = {{5, 100}};
    uint128_t b = {{5, 50}};
    TEST_ASSERT_TRUE(gt128(&a, &b));
    TEST_ASSERT_FALSE(gt128(&b, &a));

    // gte includes equality
    uint128_t c = {{5, 50}};
    TEST_ASSERT_TRUE(gte128(&b, &c));
    TEST_ASSERT_TRUE(gte128(&a, &b));
    TEST_ASSERT_FALSE(gte128(&b, &a));
}

// =============================================================================
// Shifts
// =============================================================================

void test_shiftl128_boundaries(void) {
    uint128_t n = {{0x0000000000000001ULL, 0x0000000000000001ULL}};
    uint128_t r;

    // shift by 0 = identity
    shiftl128(&n, 0, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0x1);
    TEST_ASSERT_EQUAL(LOWER(r), 0x1);

    // shift by 1 — low bit propagates up only when low overflows; here it doesn't
    shiftl128(&n, 1, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0x2);
    TEST_ASSERT_EQUAL(LOWER(r), 0x2);

    // shift by 63 — top bit of low moves to bit 63 of low, bit 0 of low moves nowhere
    uint128_t n2 = {{0x0, 0x8000000000000000ULL}};  // bit 63 of low set
    shiftl128(&n2, 1, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0x1);  // promoted into low bit of upper
    TEST_ASSERT_EQUAL(LOWER(r), 0x0);

    // shift by 64 — low becomes upper, low cleared
    uint128_t n3 = {{0xAAAA, 0xBBBB}};
    shiftl128(&n3, 64, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0xBBBB);
    TEST_ASSERT_EQUAL(LOWER(r), 0);

    // shift by 65 — same shape as 64 + shift-by-1 in upper
    shiftl128(&n3, 65, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0xBBBB << 1);
    TEST_ASSERT_EQUAL(LOWER(r), 0);

    // shift >= 128 → zero
    shiftl128(&n3, 128, &r);
    TEST_ASSERT_TRUE(zero128(&r));
    shiftl128(&n3, 200, &r);
    TEST_ASSERT_TRUE(zero128(&r));
}

void test_shiftr128_boundaries(void) {
    uint128_t n = {{0xAAAAAAAAAAAAAAAAULL, 0xBBBBBBBBBBBBBBBBULL}};
    uint128_t r;

    shiftr128(&n, 0, &r);
    TEST_ASSERT_EQUAL(UPPER(r), UPPER(n));
    TEST_ASSERT_EQUAL(LOWER(r), LOWER(n));

    // shift by 1: top bit of upper kept, low bit of upper becomes high bit of low
    shiftr128(&n, 1, &r);
    TEST_ASSERT_EQUAL(UPPER(r), UPPER(n) >> 1);
    TEST_ASSERT_EQUAL(LOWER(r), (UPPER(n) << 63) + (LOWER(n) >> 1));

    // shift by 64 → upper becomes low, upper cleared
    shiftr128(&n, 64, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0);
    TEST_ASSERT_EQUAL(LOWER(r), UPPER(n));

    // shift by 65 → 1 << of the (upper-into-low) shape
    shiftr128(&n, 65, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0);
    TEST_ASSERT_EQUAL(LOWER(r), UPPER(n) >> 1);

    // shift >= 128 → zero
    shiftr128(&n, 128, &r);
    TEST_ASSERT_TRUE(zero128(&r));
}

// =============================================================================
// bits128
// =============================================================================

void test_bits128(void) {
    uint128_t z = {{0, 0}};
    TEST_ASSERT_EQUAL(bits128(&z), 0);

    uint128_t one = {{0, 1}};
    TEST_ASSERT_EQUAL(bits128(&one), 1);

    uint128_t low_top = {{0, 0x8000000000000000ULL}};
    TEST_ASSERT_EQUAL(bits128(&low_top), 64);

    uint128_t cross = {{0x1, 0}};
    TEST_ASSERT_EQUAL(bits128(&cross), 65);

    uint128_t mx;
    memset(&mx, 0xFF, sizeof(mx));
    TEST_ASSERT_EQUAL(bits128(&mx), 128);
}

// =============================================================================
// Arithmetic
// =============================================================================

void test_add128_with_carry(void) {
    uint128_t a = {{0, 0xFFFFFFFFFFFFFFFFULL}};
    uint128_t b = {{0, 1}};
    uint128_t r;
    add128(&a, &b, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 1);  // carry propagated
    TEST_ASSERT_EQUAL(LOWER(r), 0);

    // 0 + 0 = 0
    uint128_t z = {{0, 0}};
    add128(&z, &z, &r);
    TEST_ASSERT_TRUE(zero128(&r));

    // max + 1 wraps to 0
    uint128_t mx;
    memset(&mx, 0xFF, sizeof(mx));
    add128(&mx, &b, &r);
    TEST_ASSERT_TRUE(zero128(&r));
}

void test_sub128_with_borrow(void) {
    uint128_t a = {{1, 0}};
    uint128_t b = {{0, 1}};
    uint128_t r;
    sub128(&a, &b, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0);
    TEST_ASSERT_EQUAL(LOWER(r), 0xFFFFFFFFFFFFFFFFULL);

    // x - x = 0
    sub128(&a, &a, &r);
    TEST_ASSERT_TRUE(zero128(&r));
}

void test_or128(void) {
    uint128_t a = {{0x00FF00FF00FF00FFULL, 0xAA00AA00AA00AA00ULL}};
    uint128_t b = {{0xFF00FF00FF00FF00ULL, 0x00BB00BB00BB00BBULL}};
    uint128_t r;
    or128(&a, &b, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0xFFFFFFFFFFFFFFFFULL);
    TEST_ASSERT_EQUAL(LOWER(r), 0xAABBAABBAABBAABBULL);
}

void test_mul128_small_values(void) {
    uint128_t a = {{0, 0x100000000ULL}};  // 2^32
    uint128_t b = {{0, 0x100000000ULL}};  // 2^32
    uint128_t r;
    TEST_ASSERT_TRUE(mul128(&a, &b, &r));
    // 2^32 * 2^32 = 2^64 → upper = 1, lower = 0
    TEST_ASSERT_EQUAL(UPPER(r), 1);
    TEST_ASSERT_EQUAL(LOWER(r), 0);

    // 0 * anything = 0
    uint128_t z = {{0, 0}};
    TEST_ASSERT_TRUE(mul128(&z, &a, &r));
    TEST_ASSERT_TRUE(zero128(&r));

    // 1 * anything = anything
    uint128_t one = {{0, 1}};
    uint128_t v = {{0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL}};
    TEST_ASSERT_TRUE(mul128(&one, &v, &r));
    TEST_ASSERT_EQUAL(UPPER(r), UPPER(v));
    TEST_ASSERT_EQUAL(LOWER(r), LOWER(v));
}

void test_mul128_overflow_returns_false(void) {
    // (2^128 - 1) * (2^128 - 1) overflows uint128. mul128 must detect
    // the spillover into the high 128 bits of the 256-bit product and
    // return false. Without this gate a future caller would silently
    // truncate the product (CWE-682), the same gap mul256 had before
    // its own overflow guard was added.
    uint128_t max = {{0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}};
    uint128_t r;
    // Sentinel write so we can confirm `target` is not touched on
    // failure.
    UPPER(r) = 0xAA;
    LOWER(r) = 0xBB;
    TEST_ASSERT_FALSE(mul128(&max, &max, &r));
    TEST_ASSERT_EQUAL(UPPER(r), 0xAA);
    TEST_ASSERT_EQUAL(LOWER(r), 0xBB);
}

void test_mul128_boundary_fits_in_uint128(void) {
    // 2^64 * 2^63 = 2^127 fits exactly under uint128's 2^128 ceiling.
    uint128_t a = {{1, 0}};                      // 2^64
    uint128_t b = {{0, 0x8000000000000000ULL}};  // 2^63
    uint128_t r;
    TEST_ASSERT_TRUE(mul128(&a, &b, &r));
    // 2^127 has bit 127 set: UPPER = 0x8000000000000000, LOWER = 0.
    TEST_ASSERT_EQUAL(UPPER(r), 0x8000000000000000ULL);
    TEST_ASSERT_EQUAL(LOWER(r), 0);
}

void test_divmod128_basic(void) {
    uint128_t l = {{0, 100}};
    uint128_t r_val = {{0, 7}};
    uint128_t q, rem;
    divmod128(&l, &r_val, &q, &rem);
    TEST_ASSERT_EQUAL(UPPER(q), 0);
    TEST_ASSERT_EQUAL(LOWER(q), 14);
    TEST_ASSERT_EQUAL(UPPER(rem), 0);
    TEST_ASSERT_EQUAL(LOWER(rem), 2);

    // l < r → q = 0, rem = l
    uint128_t small = {{0, 5}};
    uint128_t big = {{0, 50}};
    divmod128(&small, &big, &q, &rem);
    TEST_ASSERT_TRUE(zero128(&q));
    TEST_ASSERT_TRUE(equal128(&rem, &small));

    // l == r → q = 1, rem = 0
    uint128_t v = {{0xAA, 0xBB}};
    uint128_t v2 = {{0xAA, 0xBB}};
    divmod128(&v, &v2, &q, &rem);
    TEST_ASSERT_EQUAL(UPPER(q), 0);
    TEST_ASSERT_EQUAL(LOWER(q), 1);
    TEST_ASSERT_TRUE(zero128(&rem));
}

// =============================================================================
// String formatting
// =============================================================================

void test_tostring128_base10(void) {
    char out[40] = {0};
    uint128_t n = {{0, 12345}};
    TEST_ASSERT_TRUE(tostring128(&n, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "12345");

    // zero
    uint128_t z = {{0, 0}};
    memset(out, 0, sizeof(out));
    TEST_ASSERT_TRUE(tostring128(&z, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "0");
}

void test_tostring128_base16(void) {
    char out[40] = {0};
    uint128_t n = {{0, 0xCAFE}};
    TEST_ASSERT_TRUE(tostring128(&n, 16, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "cafe");
}

void test_tostring128_base2(void) {
    char out[150] = {0};
    uint128_t n = {{0, 0xB}};  // 0b1011
    TEST_ASSERT_TRUE(tostring128(&n, 2, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "1011");
}

void test_tostring128_invalid_base_rejected(void) {
    char out[40];
    uint128_t n = {{0, 1}};
    TEST_ASSERT_FALSE(tostring128(&n, 1, out, sizeof(out)));
    TEST_ASSERT_FALSE(tostring128(&n, 17, out, sizeof(out)));
    TEST_ASSERT_FALSE(tostring128(&n, 0, out, sizeof(out)));
}

void test_tostring128_buffer_too_small_returns_false(void) {
    char out[3];  // room for at most 2 chars + NUL
    uint128_t n = {{0, 12345}};
    TEST_ASSERT_FALSE(tostring128(&n, 10, out, sizeof(out)));
}

void test_tostring128_signed_positive_passthrough(void) {
    char out[40] = {0};
    uint128_t n = {{0, 42}};
    TEST_ASSERT_TRUE(tostring128_signed(&n, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "42");
}

void test_tostring128_signed_negative_base10(void) {
    char out[40] = {0};
    // -1 in two's complement = all bits set
    uint128_t neg_one;
    memset(&neg_one, 0xFF, sizeof(neg_one));
    TEST_ASSERT_TRUE(tostring128_signed(&neg_one, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "-1");
}

void test_tostring128_signed_non_base10_unsigned(void) {
    // In base != 10, the helper always renders as unsigned.
    char out[40] = {0};
    uint128_t neg_one;
    memset(&neg_one, 0xFF, sizeof(neg_one));
    TEST_ASSERT_TRUE(tostring128_signed(&neg_one, 16, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "ffffffffffffffffffffffffffffffff");
}

// =============================================================================
// convertUintXBE input guards
// =============================================================================

void test_convertUint128BE_full_16_bytes(void) {
    uint8_t buf[16] = {0x01,
                       0x02,
                       0x03,
                       0x04,
                       0x05,
                       0x06,
                       0x07,
                       0x08,
                       0x09,
                       0x0A,
                       0x0B,
                       0x0C,
                       0x0D,
                       0x0E,
                       0x0F,
                       0x10};
    uint128_t r = {{0xDEAD, 0xBEEF}};
    convertUint128BE(buf, 16, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0x0102030405060708ULL);
    TEST_ASSERT_EQUAL(LOWER(r), 0x090A0B0C0D0E0F10ULL);
}

void test_convertUint128BE_short_input_left_zero_padded(void) {
    uint8_t buf[2] = {0xAB, 0xCD};
    uint128_t r;
    memset(&r, 0xFF, sizeof(r));  // dirty
    convertUint128BE(buf, 2, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0);
    TEST_ASSERT_EQUAL(LOWER(r), 0xABCDULL);
}

void test_convertUint128BE_zero_length_is_noop(void) {
    uint8_t buf[1] = {0xAB};
    uint128_t r;
    memset(&r, 0x42, sizeof(r));  // dirty pattern
    convertUint128BE(buf, 0, &r);
    // Function returns early without touching target.
    TEST_ASSERT_EQUAL(UPPER(r), 0x4242424242424242ULL);
    TEST_ASSERT_EQUAL(LOWER(r), 0x4242424242424242ULL);
}

void test_convertUint128BE_oversize_length_is_noop(void) {
    uint8_t buf[20] = {0};
    uint128_t r;
    memset(&r, 0x33, sizeof(r));
    convertUint128BE(buf, 17, &r);  // > INT128_LENGTH
    TEST_ASSERT_EQUAL(UPPER(r), 0x3333333333333333ULL);
    TEST_ASSERT_EQUAL(LOWER(r), 0x3333333333333333ULL);
}

void test_convertUint128BE_null_inputs_are_noop(void) {
    uint8_t buf[4] = {1, 2, 3, 4};
    uint128_t r;
    memset(&r, 0x77, sizeof(r));
    convertUint128BE(NULL, 4, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0x7777777777777777ULL);
    convertUint128BE(buf, 4, NULL);  // must not crash
}

void test_convertUint64BEto128_small_positive(void) {
    uint8_t buf[4] = {0x00, 0x00, 0x12, 0x34};
    uint128_t r;
    convertUint64BEto128(buf, 4, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0);
    TEST_ASSERT_EQUAL(LOWER(r), 0x1234ULL);
}

void test_tostring128_exact_fit_no_room_for_nul(void) {
    // 99 in base 10 produces exactly 2 digits — the loop fills out[0] and
    // out[1] without tripping the in-loop guard, exiting with
    // offset==outLength. The post-loop guard then rejects the call
    // because there is no room left for the NUL terminator.
    char out[2];
    uint128_t n = {{0, 99}};
    TEST_ASSERT_FALSE(tostring128(&n, 10, out, sizeof(out)));
}

void test_convertUint64BEto128_oversize_length_is_noop(void) {
    // length > sizeof(tmp) (16) → early return without touching target.
    uint8_t buf[17] = {0};
    uint128_t r = {{0xDEADBEEFDEADBEEFULL, 0xCAFEBABECAFEBABEULL}};
    convertUint64BEto128(buf, 17, &r);
    // Untouched.
    TEST_ASSERT_EQUAL(UPPER(r), 0xDEADBEEFDEADBEEFULL);
    TEST_ASSERT_EQUAL(LOWER(r), 0xCAFEBABECAFEBABEULL);
}

void test_convertUint64BEto128_negative_sign_extends(void) {
    // MSB set → treated as negative → upper bytes are sign-extended (0xFF).
    uint8_t buf[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    uint128_t r;
    convertUint64BEto128(buf, 8, &r);
    TEST_ASSERT_EQUAL(UPPER(r), 0xFFFFFFFFFFFFFFFFULL);
    TEST_ASSERT_EQUAL(LOWER(r), 0xFFFFFFFFFFFFFFFEULL);
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
    RUN_TEST(test_clear128_zeros_both_halves);
    RUN_TEST(test_zero128_detects_zero_and_nonzero);
    RUN_TEST(test_copy128_preserves_both_halves);
    RUN_TEST(test_readu128BE_reads_big_endian);
    RUN_TEST(test_equal128);
    RUN_TEST(test_gt128_and_gte128);
    RUN_TEST(test_shiftl128_boundaries);
    RUN_TEST(test_shiftr128_boundaries);
    RUN_TEST(test_bits128);
    RUN_TEST(test_add128_with_carry);
    RUN_TEST(test_sub128_with_borrow);
    RUN_TEST(test_or128);
    RUN_TEST(test_mul128_small_values);
    RUN_TEST(test_mul128_overflow_returns_false);
    RUN_TEST(test_mul128_boundary_fits_in_uint128);
    RUN_TEST(test_divmod128_basic);
    RUN_TEST(test_tostring128_base10);
    RUN_TEST(test_tostring128_base16);
    RUN_TEST(test_tostring128_base2);
    RUN_TEST(test_tostring128_invalid_base_rejected);
    RUN_TEST(test_tostring128_buffer_too_small_returns_false);
    RUN_TEST(test_tostring128_signed_positive_passthrough);
    RUN_TEST(test_tostring128_signed_negative_base10);
    RUN_TEST(test_tostring128_signed_non_base10_unsigned);
    RUN_TEST(test_convertUint128BE_full_16_bytes);
    RUN_TEST(test_convertUint128BE_short_input_left_zero_padded);
    RUN_TEST(test_convertUint128BE_zero_length_is_noop);
    RUN_TEST(test_convertUint128BE_oversize_length_is_noop);
    RUN_TEST(test_convertUint128BE_null_inputs_are_noop);
    RUN_TEST(test_tostring128_exact_fit_no_room_for_nul);
    RUN_TEST(test_convertUint64BEto128_small_positive);
    RUN_TEST(test_convertUint64BEto128_oversize_length_is_noop);
    RUN_TEST(test_convertUint64BEto128_negative_sign_extends);
    return UNITY_END();
}
