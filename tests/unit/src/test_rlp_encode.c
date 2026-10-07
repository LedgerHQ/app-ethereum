/**
 * @file test_rlp_encode.c
 * @brief Unit tests for the RLP encoding primitives at
 *        src/features/sign_authorization_eip7702/rlp_encode.c.
 *
 * EIP-7702 authorization tuples are RLP-encoded on the device and the
 * resulting bytes are then hashed and signed. A bug in any of these
 * primitives changes the hash the device commits to — that means the
 * user authorizes one delegate while the chain sees a different one
 * (or the chain rejects the signature outright). The primitives are
 * tiny but every branch matters:
 *  - the zero-value short form (0x80),
 *  - the small-positive short form (number itself for 1..0x7F),
 *  - the long-form prefix (0x80 + len),
 *  - the list-header short form (0xC0 + len, len ≤ 55),
 *  - the list-header long form (0xF8 <len>, len > 55),
 *  - the output-size guard that protects every callsite from
 *    truncated buffers.
 *
 * Per the RLP spec at https://github.com/ethereum/wiki/wiki/RLP.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "rlp_encode.h"

// =============================================================================
// rlpEncodeNumber — single-byte short form
// =============================================================================

void test_encode_zero_uses_string_base(void) {
    uint8_t out[8] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    const uint8_t v = 0x00;
    uint8_t n = rlpEncodeNumber(&v, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL(n, 1);
    TEST_ASSERT_EQUAL(out[0], 0x80);
    // The function must not scribble past the encoded byte.
    TEST_ASSERT_EQUAL(out[1], 0xAA);
}

void test_encode_small_positive_is_itself(void) {
    uint8_t out[8];
    for (uint8_t v = 0x01; v <= 0x7F; v++) {
        memset(out, 0, sizeof(out));
        uint8_t n = rlpEncodeNumber(&v, 1, out, sizeof(out));
        TEST_ASSERT_EQUAL(n, 1);
        TEST_ASSERT_EQUAL(out[0], v);
    }
}

void test_encode_0x7F_is_the_short_form_boundary(void) {
    uint8_t out[8] = {0};
    const uint8_t v = 0x7F;
    uint8_t n = rlpEncodeNumber(&v, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL(n, 1);
    TEST_ASSERT_EQUAL(out[0], 0x7F);  // last value before the long-form prefix
}

void test_encode_0x80_switches_to_long_form(void) {
    uint8_t out[8] = {0};
    const uint8_t v = 0x80;
    uint8_t n = rlpEncodeNumber(&v, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL(n, 2);
    TEST_ASSERT_EQUAL(out[0], 0x81);  // RLP_STRING_BASE + 1
    TEST_ASSERT_EQUAL(out[1], 0x80);
}

void test_encode_0xFF_long_form(void) {
    uint8_t out[8] = {0};
    const uint8_t v = 0xFF;
    uint8_t n = rlpEncodeNumber(&v, 1, out, sizeof(out));
    TEST_ASSERT_EQUAL(n, 2);
    TEST_ASSERT_EQUAL(out[0], 0x81);
    TEST_ASSERT_EQUAL(out[1], 0xFF);
}

// =============================================================================
// rlpEncodeNumber — multi-byte
// =============================================================================

void test_encode_multibyte_prepends_length_prefix(void) {
    const uint8_t number[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t out[8] = {0};
    uint8_t n = rlpEncodeNumber(number, sizeof(number), out, sizeof(out));
    TEST_ASSERT_EQUAL(n, 5);
    TEST_ASSERT_EQUAL(out[0], 0x80 + 4);  // long-form prefix
    TEST_ASSERT_EQUAL_MEMORY(out + 1, number, sizeof(number));
}

void test_encode_multibyte_shift_right_in_place(void) {
    // Real callers want to encode a number that's stored later in the
    // same buffer (input is downstream of output). memmove() in the
    // source handles the right-shift safely; pin that this works.
    uint8_t buf[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0xCC, 0xCC, 0xCC};
    uint8_t n = rlpEncodeNumber(buf + 1, 4, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(n, 5);
    TEST_ASSERT_EQUAL(buf[0], 0x84);
    TEST_ASSERT_EQUAL(buf[1], 0x11);
    TEST_ASSERT_EQUAL(buf[2], 0x22);
    TEST_ASSERT_EQUAL(buf[3], 0x33);
    TEST_ASSERT_EQUAL(buf[4], 0x44);
}

// =============================================================================
// rlpEncodeNumber — output buffer guard
// =============================================================================

void test_encode_rejects_undersized_buffer(void) {
    const uint8_t v = 0x42;
    uint8_t out[2] = {0xCC, 0xCC};
    // The guard is `output_size < numberLength + 2`, so a buffer of
    // exactly numberLength + 1 (the actual maximum needed) is refused.
    // That's the conservative guard the source enforces — pin it.
    uint8_t n = rlpEncodeNumber(&v, 1, out, 2);
    TEST_ASSERT_EQUAL(n, 0);
    TEST_ASSERT_EQUAL(out[0], 0xCC);  // untouched
}

void test_encode_accepts_buffer_at_guard_boundary(void) {
    const uint8_t v = 0x42;
    uint8_t out[3] = {0xCC, 0xCC, 0xCC};
    uint8_t n = rlpEncodeNumber(&v, 1, out, 3);  // numberLength + 2
    TEST_ASSERT_EQUAL(n, 1);
    TEST_ASSERT_EQUAL(out[0], 0x42);
}

void test_encode_multibyte_rejects_undersized_buffer(void) {
    const uint8_t number[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t out[5] = {0};
    uint8_t n = rlpEncodeNumber(number, sizeof(number), out, sizeof(out));  // < 4 + 2
    TEST_ASSERT_EQUAL(n, 0);
}

// =============================================================================
// rlpGetEncodedNumberLength
// =============================================================================

void test_get_length_short_form(void) {
    const uint8_t v0 = 0x00;
    const uint8_t v1 = 0x01;
    const uint8_t v7f = 0x7F;
    // The function does NOT special-case 0 (unlike rlpEncodeNumber),
    // so 0 reports length 1 (matching the encoded short form 0x80).
    TEST_ASSERT_EQUAL(rlpGetEncodedNumberLength(&v0, 1), 1);
    TEST_ASSERT_EQUAL(rlpGetEncodedNumberLength(&v1, 1), 1);
    TEST_ASSERT_EQUAL(rlpGetEncodedNumberLength(&v7f, 1), 1);
}

void test_get_length_long_form_one_byte(void) {
    const uint8_t v80 = 0x80;
    const uint8_t vff = 0xFF;
    TEST_ASSERT_EQUAL(rlpGetEncodedNumberLength(&v80, 1), 2);
    TEST_ASSERT_EQUAL(rlpGetEncodedNumberLength(&vff, 1), 2);
}

void test_get_length_multibyte(void) {
    const uint8_t v[4] = {0x12, 0x34, 0x56, 0x78};
    TEST_ASSERT_EQUAL(rlpGetEncodedNumberLength(v, 4), 5);
    TEST_ASSERT_EQUAL(rlpGetEncodedNumberLength(v, 2), 3);
}

// =============================================================================
// rlpGetSmallestNumber64EncodingSize
// =============================================================================

void test_smallest_size_zero_reports_zero(void) {
    // 0 needs *no* significant bytes to be represented. The helper
    // reports 0 in this corner case (the `while (number)` loop never
    // runs). Callers must use rlpGetEncodedNumber64Length() to obtain
    // the actual RLP-encoded length, which special-cases 0 → 1.
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0), 0);
}

void test_smallest_size_byte_boundaries(void) {
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0x01), 1);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0x7F), 1);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0x80), 1);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0xFF), 1);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0x100), 2);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0xFFFF), 2);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0x10000), 3);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0xFFFFFFFF), 4);
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0x100000000ULL), 5);
}

void test_smallest_size_u64_max(void) {
    TEST_ASSERT_EQUAL(rlpGetSmallestNumber64EncodingSize(0xFFFFFFFFFFFFFFFFULL), 8);
}

// =============================================================================
// rlpGetEncodedNumber64Length
// =============================================================================

void test_encoded_64_length_zero(void) {
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0), 1);
}

void test_encoded_64_length_short_form_boundary(void) {
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0x7F), 1);
    // 0x80 is the first value that needs the long-form prefix.
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0x80), 2);
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0xFF), 2);
}

void test_encoded_64_length_multibyte(void) {
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0x100), 3);  // 2 bytes + prefix
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0xFFFF), 3);
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0xFFFFFFFFULL), 5);
    TEST_ASSERT_EQUAL(rlpGetEncodedNumber64Length(0xFFFFFFFFFFFFFFFFULL), 9);
}

// =============================================================================
// rlpEncodeListHeader8
// =============================================================================

void test_list_header_short_form(void) {
    uint8_t out[2] = {0};
    TEST_ASSERT_EQUAL(rlpEncodeListHeader8(0, out, sizeof(out)), 1);
    TEST_ASSERT_EQUAL(out[0], 0xC0);
}

void test_list_header_short_form_boundary(void) {
    uint8_t out[2] = {0};
    // size = 55 is the last value that fits in the short form.
    TEST_ASSERT_EQUAL(rlpEncodeListHeader8(55, out, sizeof(out)), 1);
    TEST_ASSERT_EQUAL(out[0], 0xC0 + 55);  // 0xF7
}

void test_list_header_long_form_switch(void) {
    uint8_t out[2] = {0};
    // size = 56 triggers the long form (0xF8 <size>).
    TEST_ASSERT_EQUAL(rlpEncodeListHeader8(56, out, sizeof(out)), 2);
    TEST_ASSERT_EQUAL(out[0], 0xF8);
    TEST_ASSERT_EQUAL(out[1], 56);
}

void test_list_header_long_form_max(void) {
    uint8_t out[2] = {0};
    TEST_ASSERT_EQUAL(rlpEncodeListHeader8(255, out, sizeof(out)), 2);
    TEST_ASSERT_EQUAL(out[0], 0xF8);
    TEST_ASSERT_EQUAL(out[1], 255);
}

void test_list_header_rejects_undersized_buffer(void) {
    uint8_t out[1] = {0xCC};
    // The guard is unconditional: output_size < 2 ⇒ 0, even for the
    // short form that would only need 1 byte.
    TEST_ASSERT_EQUAL(rlpEncodeListHeader8(0, out, 1), 0);
    TEST_ASSERT_EQUAL(out[0], 0xCC);
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
    RUN_TEST(test_encode_zero_uses_string_base);
    RUN_TEST(test_encode_small_positive_is_itself);
    RUN_TEST(test_encode_0x7F_is_the_short_form_boundary);
    RUN_TEST(test_encode_0x80_switches_to_long_form);
    RUN_TEST(test_encode_0xFF_long_form);
    RUN_TEST(test_encode_multibyte_prepends_length_prefix);
    RUN_TEST(test_encode_multibyte_shift_right_in_place);
    RUN_TEST(test_encode_rejects_undersized_buffer);
    RUN_TEST(test_encode_accepts_buffer_at_guard_boundary);
    RUN_TEST(test_encode_multibyte_rejects_undersized_buffer);
    RUN_TEST(test_get_length_short_form);
    RUN_TEST(test_get_length_long_form_one_byte);
    RUN_TEST(test_get_length_multibyte);
    RUN_TEST(test_smallest_size_zero_reports_zero);
    RUN_TEST(test_smallest_size_byte_boundaries);
    RUN_TEST(test_smallest_size_u64_max);
    RUN_TEST(test_encoded_64_length_zero);
    RUN_TEST(test_encoded_64_length_short_form_boundary);
    RUN_TEST(test_encoded_64_length_multibyte);
    RUN_TEST(test_list_header_short_form);
    RUN_TEST(test_list_header_short_form_boundary);
    RUN_TEST(test_list_header_long_form_switch);
    RUN_TEST(test_list_header_long_form_max);
    RUN_TEST(test_list_header_rejects_undersized_buffer);
    return UNITY_END();
}
