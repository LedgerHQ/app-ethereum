/**
 * @file test_uint256.c
 * @brief Unit tests for uint256_t arithmetic helpers in src/uint256.c.
 *
 * uint256_t is the canonical EVM amount, used everywhere the device
 * formats a balance, fee, or token quantity for display. The helpers
 * stack on top of uint128_t (already covered by test_uint128.c) so this
 * suite focuses on the 256-bit-specific bookkeeping:
 *   - the shift / arith routines that have to propagate carry/borrow
 *     across the 128-bit halves,
 *   - tostring256 output-buffer-too-small behavior (prints "..." instead
 *     of overrunning),
 *   - mul256 byte-marshalling and error propagation, both verified with
 *     a linker --wrap on cx_math_mult_no_throw.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "uint256.h"
#include "uint_common.h"

// =============================================================================
// __wrap stubs for cx_math_mult_no_throw
// =============================================================================
//
// mul256() converts both inputs to 32-byte BE buffers, asks the SDK to
// multiply them, and reads back the lower 256 bits of the 64-byte BE result.
// To verify the byte plumbing without depending on the SDK, the wrap returns
// pre-canned 64-byte buffers selected by the test via `mock()`.

#define CX_OK                0
#define CX_INVALID_PARAMETER 0xFFFF

// Static variables controlling the cx_math_mult_no_throw stub.
static uint32_t g_mult_ret = CX_OK;
static const uint8_t *g_mult_canned = NULL;

uint32_t cx_math_mult_no_throw(uint8_t *r, const uint8_t *a, const uint8_t *b, size_t len) {
    (void) a;
    (void) b;
    if (g_mult_canned != NULL) {
        memcpy(r, g_mult_canned, 2 * len);
    }
    return g_mult_ret;
}

// =============================================================================
// Constructors / accessors
// =============================================================================

void test_clear256_zeros_all_four_limbs(void) {
    uint256_t n;
    memset(&n, 0xCC, sizeof(n));
    clear256(&n);
    TEST_ASSERT_TRUE(zero256(&n));
}

void test_zero256_detects_zero_and_nonzero(void) {
    uint256_t z;
    clear256(&z);
    TEST_ASSERT_TRUE(zero256(&z));

    uint256_t low_only;
    clear256(&low_only);
    LOWER(LOWER(low_only)) = 1;
    TEST_ASSERT_FALSE(zero256(&low_only));

    uint256_t high_only;
    clear256(&high_only);
    UPPER(UPPER(high_only)) = 1;
    TEST_ASSERT_FALSE(zero256(&high_only));
}

void test_copy256_preserves_all_four_limbs(void) {
    uint256_t src;
    uint256_t dst;
    UPPER(UPPER(src)) = 0x1111111111111111ULL;
    LOWER(UPPER(src)) = 0x2222222222222222ULL;
    UPPER(LOWER(src)) = 0x3333333333333333ULL;
    LOWER(LOWER(src)) = 0x4444444444444444ULL;
    clear256(&dst);
    copy256(&dst, &src);
    TEST_ASSERT_TRUE(equal256(&dst, &src));
}

void test_readu256BE_reads_big_endian(void) {
    uint8_t buf[32];
    for (size_t i = 0; i < 32; i++) {
        buf[i] = (uint8_t) (i + 1);
    }
    uint256_t n;
    readu256BE(buf, &n);
    TEST_ASSERT_EQUAL(UPPER(UPPER(n)), 0x0102030405060708ULL);
    TEST_ASSERT_EQUAL(LOWER(UPPER(n)), 0x090A0B0C0D0E0F10ULL);
    TEST_ASSERT_EQUAL(UPPER(LOWER(n)), 0x1112131415161718ULL);
    TEST_ASSERT_EQUAL(LOWER(LOWER(n)), 0x191A1B1C1D1E1F20ULL);
}

// =============================================================================
// Comparison
// =============================================================================

void test_equal256_and_ordering(void) {
    uint256_t a;
    uint256_t b;
    clear256(&a);
    clear256(&b);
    UPPER(UPPER(a)) = 1;
    UPPER(UPPER(b)) = 1;
    TEST_ASSERT_TRUE(equal256(&a, &b));

    LOWER(LOWER(b)) = 1;
    TEST_ASSERT_FALSE(equal256(&a, &b));
    TEST_ASSERT_TRUE(gt256(&b, &a));
    TEST_ASSERT_TRUE(gte256(&b, &a));
    TEST_ASSERT_FALSE(gt256(&a, &b));

    // High-limb dominance: even with high LOWER, the higher UPPER wins.
    uint256_t big;
    uint256_t small;
    clear256(&big);
    clear256(&small);
    UPPER(UPPER(big)) = 2;
    UPPER(UPPER(small)) = 1;
    LOWER(LOWER(small)) = 0xFFFFFFFFFFFFFFFFULL;
    TEST_ASSERT_TRUE(gt256(&big, &small));
}

// =============================================================================
// Shifts — boundaries are 0, 1, 127, 128, 129, 255, 256, > 256
// =============================================================================

void test_shiftl256_boundaries(void) {
    uint256_t n;
    uint256_t r;
    clear256(&n);
    LOWER(LOWER(n)) = 1;  // 256-bit value "1"

    // shift by 0 = identity
    shiftl256(&n, 0, &r);
    TEST_ASSERT_TRUE(equal256(&r, &n));

    // shift by 1 = 2
    shiftl256(&n, 1, &r);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 2);
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0);

    // shift by 128 — low-128 moves into upper-128
    clear256(&n);
    LOWER(LOWER(n)) = 0xAAAAAAAAAAAAAAAAULL;
    UPPER(LOWER(n)) = 0xBBBBBBBBBBBBBBBBULL;
    shiftl256(&n, 128, &r);
    TEST_ASSERT_EQUAL(LOWER(UPPER(r)), 0xAAAAAAAAAAAAAAAAULL);
    TEST_ASSERT_EQUAL(UPPER(UPPER(r)), 0xBBBBBBBBBBBBBBBBULL);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 0);
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0);

    // shift by 256 = clear
    shiftl256(&n, 256, &r);
    TEST_ASSERT_TRUE(zero256(&r));

    // shift > 256 = clear
    shiftl256(&n, 300, &r);
    TEST_ASSERT_TRUE(zero256(&r));
}

void test_shiftr256_boundaries(void) {
    uint256_t n;
    uint256_t r;
    memset(&n, 0xFF, sizeof(n));  // all ones

    // shift by 0 = identity
    shiftr256(&n, 0, &r);
    TEST_ASSERT_TRUE(equal256(&r, &n));

    // shift by 128 — upper-128 moves into lower-128
    shiftr256(&n, 128, &r);
    TEST_ASSERT_EQUAL(UPPER(UPPER(r)), 0);
    TEST_ASSERT_EQUAL(LOWER(UPPER(r)), 0);
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0xFFFFFFFFFFFFFFFFULL);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 0xFFFFFFFFFFFFFFFFULL);

    // shift by 255 — only the MSB survives in bit 0
    shiftr256(&n, 255, &r);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 1);
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0);
    TEST_ASSERT_TRUE(zero128(&UPPER(r)));

    // shift >= 256 = clear
    shiftr256(&n, 256, &r);
    TEST_ASSERT_TRUE(zero256(&r));
}

// =============================================================================
// bits256
// =============================================================================

void test_bits256(void) {
    uint256_t z;
    clear256(&z);
    TEST_ASSERT_EQUAL(bits256(&z), 0);

    uint256_t one;
    clear256(&one);
    LOWER(LOWER(one)) = 1;
    TEST_ASSERT_EQUAL(bits256(&one), 1);

    uint256_t low_top;
    clear256(&low_top);
    UPPER(LOWER(low_top)) = 0x8000000000000000ULL;
    TEST_ASSERT_EQUAL(bits256(&low_top), 128);

    uint256_t cross;
    clear256(&cross);
    LOWER(UPPER(cross)) = 1;
    TEST_ASSERT_EQUAL(bits256(&cross), 129);

    uint256_t mx;
    memset(&mx, 0xFF, sizeof(mx));
    TEST_ASSERT_EQUAL(bits256(&mx), 256);
}

// =============================================================================
// Arithmetic — focus on cross-128 carry/borrow
// =============================================================================

void test_add256_carries_across_128_boundary(void) {
    uint256_t a;
    uint256_t b;
    uint256_t r;
    clear256(&a);
    clear256(&b);
    // a = 2^128 - 1, b = 1 → r = 2^128 (carry propagates across the boundary)
    UPPER(LOWER(a)) = 0xFFFFFFFFFFFFFFFFULL;
    LOWER(LOWER(a)) = 0xFFFFFFFFFFFFFFFFULL;
    LOWER(LOWER(b)) = 1;
    add256(&a, &b, &r);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 0);
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0);
    TEST_ASSERT_EQUAL(LOWER(UPPER(r)), 1);
    TEST_ASSERT_EQUAL(UPPER(UPPER(r)), 0);

    // max + 1 wraps to zero
    uint256_t mx;
    memset(&mx, 0xFF, sizeof(mx));
    add256(&mx, &b, &r);
    TEST_ASSERT_TRUE(zero256(&r));
}

void test_sub256_borrows_across_128_boundary(void) {
    uint256_t a;
    uint256_t b;
    uint256_t r;
    clear256(&a);
    clear256(&b);
    LOWER(UPPER(a)) = 1;  // a = 2^128
    LOWER(LOWER(b)) = 1;  // b = 1
    sub256(&a, &b, &r);
    // r should be 2^128 - 1: lower-128 all ones, upper-128 zero
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 0xFFFFFFFFFFFFFFFFULL);
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0xFFFFFFFFFFFFFFFFULL);
    TEST_ASSERT_TRUE(zero128(&UPPER(r)));
}

void test_or256(void) {
    uint256_t a;
    uint256_t b;
    uint256_t r;
    clear256(&a);
    clear256(&b);
    UPPER(UPPER(a)) = 0xAAAA;
    LOWER(LOWER(b)) = 0xBBBB;
    or256(&a, &b, &r);
    TEST_ASSERT_EQUAL(UPPER(UPPER(r)), 0xAAAA);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 0xBBBB);
}

void test_divmod256_basic(void) {
    uint256_t l;
    uint256_t r_val;
    uint256_t q;
    uint256_t rem;
    clear256(&l);
    clear256(&r_val);
    LOWER(LOWER(l)) = 100;
    LOWER(LOWER(r_val)) = 7;
    divmod256(&l, &r_val, &q, &rem);
    TEST_ASSERT_EQUAL(LOWER(LOWER(q)), 14);
    TEST_ASSERT_TRUE(zero128(&UPPER(q)));
    TEST_ASSERT_EQUAL(LOWER(LOWER(rem)), 2);

    // l < r → q = 0, rem = l
    uint256_t big;
    clear256(&big);
    LOWER(LOWER(big)) = 50;
    uint256_t small;
    clear256(&small);
    LOWER(LOWER(small)) = 5;
    divmod256(&small, &big, &q, &rem);
    TEST_ASSERT_TRUE(zero256(&q));
    TEST_ASSERT_TRUE(equal256(&rem, &small));

    // l == r → q = 1, rem = 0
    divmod256(&l, &l, &q, &rem);
    TEST_ASSERT_EQUAL(LOWER(LOWER(q)), 1);
    TEST_ASSERT_TRUE(zero256(&rem));
}

// =============================================================================
// String formatting
// =============================================================================

void test_tostring256_base10(void) {
    char out[80] = {0};
    uint256_t n;
    clear256(&n);
    LOWER(LOWER(n)) = 1234567890ULL;
    TEST_ASSERT_TRUE(tostring256(&n, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "1234567890");
}

void test_tostring256_zero(void) {
    char out[80] = {0};
    uint256_t z;
    clear256(&z);
    TEST_ASSERT_TRUE(tostring256(&z, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "0");
}

void test_tostring256_base16_max(void) {
    char out[80] = {0};
    uint256_t mx;
    memset(&mx, 0xFF, sizeof(mx));
    TEST_ASSERT_TRUE(tostring256(&mx, 16, out, sizeof(out)));
    // 2^256 - 1 = 64 'f' characters
    TEST_ASSERT_EQUAL(strlen(out), 64);
    for (size_t i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL(out[i], 'f');
    }
}

void test_tostring256_invalid_base_rejected(void) {
    char out[80];
    uint256_t n;
    clear256(&n);
    LOWER(LOWER(n)) = 1;
    TEST_ASSERT_FALSE(tostring256(&n, 1, out, sizeof(out)));
    TEST_ASSERT_FALSE(tostring256(&n, 17, out, sizeof(out)));
    TEST_ASSERT_FALSE(tostring256(&n, 0, out, sizeof(out)));
}

void test_tostring256_zero_outlength_rejected(void) {
    char out[1];
    uint256_t n;
    clear256(&n);
    LOWER(LOWER(n)) = 1;
    TEST_ASSERT_FALSE(tostring256(&n, 10, out, 0));
}

void test_tostring256_buffer_too_small_writes_ellipsis(void) {
    // The number prints "12345" (5 digits + NUL = 6 bytes) but the buffer is
    // only 5. The function should fail AND leave "..." in the buffer as a
    // visible signal — that's the contract.
    char out[5];
    memset(out, 'X', sizeof(out));
    uint256_t n;
    clear256(&n);
    LOWER(LOWER(n)) = 12345;
    TEST_ASSERT_FALSE(tostring256(&n, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "...");
}

void test_tostring256_tiny_buffer_clears(void) {
    // outLength <= 3 → no room for "..." either, just terminate empty.
    char out[3] = {'X', 'X', 'X'};
    uint256_t n;
    clear256(&n);
    LOWER(LOWER(n)) = 12345;
    TEST_ASSERT_FALSE(tostring256(&n, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL(out[0], '\0');
}

void test_tostring256_signed_positive_passthrough(void) {
    char out[80] = {0};
    uint256_t n;
    clear256(&n);
    LOWER(LOWER(n)) = 42;
    TEST_ASSERT_TRUE(tostring256_signed(&n, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "42");
}

void test_tostring256_signed_negative_one(void) {
    char out[80] = {0};
    uint256_t neg_one;
    memset(&neg_one, 0xFF, sizeof(neg_one));
    TEST_ASSERT_TRUE(tostring256_signed(&neg_one, 10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "-1");
}

void test_tostring256_signed_non_base10_unsigned(void) {
    char out[80] = {0};
    uint256_t neg_one;
    memset(&neg_one, 0xFF, sizeof(neg_one));
    TEST_ASSERT_TRUE(tostring256_signed(&neg_one, 16, out, sizeof(out)));
    // In base 16 the value is rendered as 2^256 - 1, i.e. 64 'f's, regardless of sign.
    TEST_ASSERT_EQUAL(strlen(out), 64);
}

// =============================================================================
// convertUint256BE input guards
// =============================================================================

void test_convertUint256BE_full_32_bytes(void) {
    uint8_t buf[32];
    for (size_t i = 0; i < 32; i++) {
        buf[i] = (uint8_t) i;
    }
    uint256_t r;
    memset(&r, 0xDD, sizeof(r));
    convertUint256BE(buf, 32, &r);
    TEST_ASSERT_EQUAL(UPPER(UPPER(r)), 0x0001020304050607ULL);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 0x18191A1B1C1D1E1FULL);
}

void test_convertUint256BE_short_input_left_zero_padded(void) {
    uint8_t buf[2] = {0xAB, 0xCD};
    uint256_t r;
    memset(&r, 0xFF, sizeof(r));
    convertUint256BE(buf, 2, &r);
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 0xABCDULL);
    TEST_ASSERT_TRUE(zero128(&UPPER(r)));
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0);
}

void test_convertUint256BE_zero_length_is_noop(void) {
    uint8_t buf[1] = {0xAB};
    uint256_t r;
    memset(&r, 0x42, sizeof(r));
    convertUint256BE(buf, 0, &r);
    // every byte still 0x42
    for (size_t i = 0; i < sizeof(r); i++) {
        TEST_ASSERT_EQUAL(((uint8_t *) &r)[i], 0x42);
    }
}

void test_convertUint256BE_oversize_length_is_noop(void) {
    uint8_t buf[40] = {0};
    uint256_t r;
    memset(&r, 0x77, sizeof(r));
    convertUint256BE(buf, 33, &r);
    for (size_t i = 0; i < sizeof(r); i++) {
        TEST_ASSERT_EQUAL(((uint8_t *) &r)[i], 0x77);
    }
}

void test_convertUint256BE_null_inputs_are_noop(void) {
    uint8_t buf[4] = {1, 2, 3, 4};
    uint256_t r;
    memset(&r, 0x77, sizeof(r));
    convertUint256BE(NULL, 4, &r);
    TEST_ASSERT_EQUAL(((uint8_t *) &r)[0], 0x77);
    convertUint256BE(buf, 4, NULL);  // must not crash
}

// =============================================================================
// mul256 — byte marshalling & error propagation (wrapped SDK call)
// =============================================================================

void test_mul256_success_marshals_lower_half(void) {
    // Pre-canned 64-byte BE result: lower 32 bytes = 0x...000001
    static uint8_t canned[64];
    memset(canned, 0, sizeof(canned));
    canned[63] = 1;

    uint256_t a;
    uint256_t b;
    uint256_t r;
    memset(&a, 0xAA, sizeof(a));
    memset(&b, 0xBB, sizeof(b));
    clear256(&r);

    g_mult_ret = CX_OK;
    g_mult_canned = canned;

    TEST_ASSERT_TRUE(mul256(&a, &b, &r));
    // mul256 reads result[32..63] back as BE → only LOWER(LOWER(r)) = 1
    TEST_ASSERT_EQUAL(LOWER(LOWER(r)), 1);
    TEST_ASSERT_EQUAL(UPPER(LOWER(r)), 0);
    TEST_ASSERT_TRUE(zero128(&UPPER(r)));
}

void test_mul256_returns_false_on_sdk_error(void) {
    uint256_t a;
    uint256_t b;
    uint256_t r;
    clear256(&a);
    clear256(&b);
    clear256(&r);

    g_mult_ret = CX_INVALID_PARAMETER;
    g_mult_canned = NULL;

    TEST_ASSERT_FALSE(mul256(&a, &b, &r));
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
    RUN_TEST(test_clear256_zeros_all_four_limbs);
    RUN_TEST(test_zero256_detects_zero_and_nonzero);
    RUN_TEST(test_copy256_preserves_all_four_limbs);
    RUN_TEST(test_readu256BE_reads_big_endian);
    RUN_TEST(test_equal256_and_ordering);
    RUN_TEST(test_shiftl256_boundaries);
    RUN_TEST(test_shiftr256_boundaries);
    RUN_TEST(test_bits256);
    RUN_TEST(test_add256_carries_across_128_boundary);
    RUN_TEST(test_sub256_borrows_across_128_boundary);
    RUN_TEST(test_or256);
    RUN_TEST(test_divmod256_basic);
    RUN_TEST(test_tostring256_base10);
    RUN_TEST(test_tostring256_zero);
    RUN_TEST(test_tostring256_base16_max);
    RUN_TEST(test_tostring256_invalid_base_rejected);
    RUN_TEST(test_tostring256_zero_outlength_rejected);
    RUN_TEST(test_tostring256_buffer_too_small_writes_ellipsis);
    RUN_TEST(test_tostring256_tiny_buffer_clears);
    RUN_TEST(test_tostring256_signed_positive_passthrough);
    RUN_TEST(test_tostring256_signed_negative_one);
    RUN_TEST(test_tostring256_signed_non_base10_unsigned);
    RUN_TEST(test_convertUint256BE_full_32_bytes);
    RUN_TEST(test_convertUint256BE_short_input_left_zero_padded);
    RUN_TEST(test_convertUint256BE_zero_length_is_noop);
    RUN_TEST(test_convertUint256BE_oversize_length_is_noop);
    RUN_TEST(test_convertUint256BE_null_inputs_are_noop);
    RUN_TEST(test_mul256_success_marshals_lower_half);
    RUN_TEST(test_mul256_returns_false_on_sdk_error);
    return UNITY_END();
}
