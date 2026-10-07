/**
 * @file test_rlp_utils.c
 * @brief Unit tests for RLP length-prefix decoding helpers.
 *
 * Covers `rlp_can_decode()` and `rlp_decode_length()` from
 * `src/features/sign_tx/rlp_utils.c`. Both helpers parse the leading byte(s)
 * of an RLP-encoded field and need to behave safely on truncated input — the
 * fix in commit ad3fb1fc made `rlp_decode_length()` self-bounded after a
 * security review.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "rlp_utils.h"

// =============================================================================
// rlp_can_decode — bufferLength sanity for the long-string/long-list cases
// =============================================================================
//
// rlp_can_decode() dereferences *buffer unconditionally, so its contract
// requires bufferLength >= 1. We respect that here.

void test_can_decode_single_byte(void) {
    uint8_t buf[] = {0x00};
    bool valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_TRUE(valid);

    buf[0] = 0x7f;
    valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_TRUE(valid);
}

void test_can_decode_short_string(void) {
    uint8_t buf[] = {0x80};
    bool valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_TRUE(valid);

    buf[0] = 0xb7;
    valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_TRUE(valid);
}

void test_can_decode_short_list(void) {
    uint8_t buf[] = {0xc0};
    bool valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_TRUE(valid);

    buf[0] = 0xf7;
    valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_TRUE(valid);
}

void test_can_decode_long_string_truncated_returns_false(void) {
    // 0xb8 announces 1 byte of length to follow → need 2 bytes total.
    uint8_t buf[] = {0xb8};
    bool valid = true;
    TEST_ASSERT_FALSE(rlp_can_decode(buf, sizeof(buf), &valid));

    // 0xbb announces 4 bytes of length → need 5 bytes total.
    uint8_t buf2[] = {0xbb, 0x00, 0x00, 0x00};
    valid = true;
    TEST_ASSERT_FALSE(rlp_can_decode(buf2, sizeof(buf2), &valid));
}

void test_can_decode_long_string_complete_returns_valid(void) {
    uint8_t buf[] = {0xb8, 0x10};  // length = 16
    bool valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_TRUE(valid);

    uint8_t buf2[] = {0xbb, 0x01, 0x02, 0x03, 0x04};
    valid = false;
    TEST_ASSERT_TRUE(rlp_can_decode(buf2, sizeof(buf2), &valid));
    TEST_ASSERT_TRUE(valid);
}

void test_can_decode_long_string_over_4_bytes_flags_invalid(void) {
    // 0xbc would announce 5 bytes of length — over the app's 32-bit limit.
    uint8_t buf[] = {0xbc, 0, 0, 0, 0, 0};
    bool valid = true;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_FALSE(valid);

    uint8_t buf2[] = {0xbf, 0, 0, 0, 0, 0, 0, 0, 0};
    valid = true;
    TEST_ASSERT_TRUE(rlp_can_decode(buf2, sizeof(buf2), &valid));
    TEST_ASSERT_FALSE(valid);
}

void test_can_decode_long_list_truncated_returns_false(void) {
    uint8_t buf[] = {0xf8};
    bool valid = true;
    TEST_ASSERT_FALSE(rlp_can_decode(buf, sizeof(buf), &valid));

    uint8_t buf2[] = {0xfb, 0x00, 0x00, 0x00};
    valid = true;
    TEST_ASSERT_FALSE(rlp_can_decode(buf2, sizeof(buf2), &valid));
}

void test_can_decode_long_list_over_4_bytes_flags_invalid(void) {
    uint8_t buf[] = {0xfc, 0, 0, 0, 0, 0};
    bool valid = true;
    TEST_ASSERT_TRUE(rlp_can_decode(buf, sizeof(buf), &valid));
    TEST_ASSERT_FALSE(valid);

    uint8_t buf2[] = {0xff, 0, 0, 0, 0, 0, 0, 0, 0};
    valid = true;
    TEST_ASSERT_TRUE(rlp_can_decode(buf2, sizeof(buf2), &valid));
    TEST_ASSERT_FALSE(valid);
}

// =============================================================================
// rlp_decode_length — happy paths
// =============================================================================

void test_decode_single_byte(void) {
    uint8_t buf[] = {0x00};
    uint32_t field_length = 0xDEAD;
    uint32_t offset = 0xDEAD;
    bool list = true;

    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 0);
    TEST_ASSERT_EQUAL(field_length, 1);
    TEST_ASSERT_FALSE(list);

    buf[0] = 0x7f;
    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 0);
    TEST_ASSERT_EQUAL(field_length, 1);
    TEST_ASSERT_FALSE(list);
}

void test_decode_short_string(void) {
    // 0x80 → empty string, length 0
    uint8_t buf[] = {0x80};
    uint32_t field_length = 0xDEAD;
    uint32_t offset = 0xDEAD;
    bool list = true;
    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 1);
    TEST_ASSERT_EQUAL(field_length, 0);
    TEST_ASSERT_FALSE(list);

    // 0xb7 → 55-byte string
    buf[0] = 0xb7;
    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 1);
    TEST_ASSERT_EQUAL(field_length, 0x37);
    TEST_ASSERT_FALSE(list);
}

void test_decode_long_string_1_byte_length(void) {
    uint8_t buf[] = {0xb8, 0xAB};
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = true;

    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 2);
    TEST_ASSERT_EQUAL(field_length, 0xAB);
    TEST_ASSERT_FALSE(list);
}

void test_decode_long_string_2_byte_length(void) {
    uint8_t buf[] = {0xb9, 0x12, 0x34};
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = true;

    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 3);
    TEST_ASSERT_EQUAL(field_length, 0x1234);
    TEST_ASSERT_FALSE(list);
}

void test_decode_long_string_3_byte_length(void) {
    uint8_t buf[] = {0xba, 0x12, 0x34, 0x56};
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = true;

    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 4);
    TEST_ASSERT_EQUAL(field_length, 0x123456);
    TEST_ASSERT_FALSE(list);
}

void test_decode_long_string_4_byte_length(void) {
    // Use the high bit to verify uint32 promotion (no sign extension bug).
    uint8_t buf[] = {0xbb, 0x80, 0x00, 0x00, 0x01};
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = true;

    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 5);
    TEST_ASSERT_EQUAL(field_length, 0x80000001u);
    TEST_ASSERT_FALSE(list);
}

void test_decode_short_list(void) {
    uint8_t buf[] = {0xc0};
    uint32_t field_length = 0xDEAD;
    uint32_t offset = 0xDEAD;
    bool list = false;
    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 1);
    TEST_ASSERT_EQUAL(field_length, 0);
    TEST_ASSERT_TRUE(list);

    buf[0] = 0xf7;
    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 1);
    TEST_ASSERT_EQUAL(field_length, 0x37);
    TEST_ASSERT_TRUE(list);
}

void test_decode_long_list_1_byte_length(void) {
    uint8_t buf[] = {0xf8, 0xCD};
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = false;

    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 2);
    TEST_ASSERT_EQUAL(field_length, 0xCD);
    TEST_ASSERT_TRUE(list);
}

void test_decode_long_list_4_byte_length(void) {
    uint8_t buf[] = {0xfb, 0xFF, 0xFF, 0xFF, 0xFE};
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = false;

    TEST_ASSERT_TRUE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    TEST_ASSERT_EQUAL(offset, 5);
    TEST_ASSERT_EQUAL(field_length, 0xFFFFFFFEu);
    TEST_ASSERT_TRUE(list);
}

// =============================================================================
// rlp_decode_length — bound checks (regression for fix ad3fb1fc)
// =============================================================================

void test_decode_empty_buffer_rejected(void) {
    // The fix's most important invariant: never deref buffer when bufferLength == 0.
    uint8_t buf[1] = {0};
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = false;

    TEST_ASSERT_FALSE(rlp_decode_length(buf, 0, &field_length, &offset, &list));
}

void test_decode_long_string_truncated_rejected(void) {
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = false;

    // 0xb8 needs 2 bytes, only 1 provided
    uint8_t buf1[] = {0xb8};
    TEST_ASSERT_FALSE(rlp_decode_length(buf1, sizeof(buf1), &field_length, &offset, &list));

    // 0xb9 needs 3 bytes, only 2 provided
    uint8_t buf2[] = {0xb9, 0x00};
    TEST_ASSERT_FALSE(rlp_decode_length(buf2, sizeof(buf2), &field_length, &offset, &list));

    // 0xba needs 4 bytes, only 3 provided
    uint8_t buf3[] = {0xba, 0x00, 0x00};
    TEST_ASSERT_FALSE(rlp_decode_length(buf3, sizeof(buf3), &field_length, &offset, &list));

    // 0xbb needs 5 bytes, only 4 provided
    uint8_t buf4[] = {0xbb, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(rlp_decode_length(buf4, sizeof(buf4), &field_length, &offset, &list));
}

void test_decode_long_list_truncated_rejected(void) {
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = false;

    uint8_t buf1[] = {0xf8};
    TEST_ASSERT_FALSE(rlp_decode_length(buf1, sizeof(buf1), &field_length, &offset, &list));

    uint8_t buf2[] = {0xf9, 0x00};
    TEST_ASSERT_FALSE(rlp_decode_length(buf2, sizeof(buf2), &field_length, &offset, &list));

    uint8_t buf3[] = {0xfa, 0x00, 0x00};
    TEST_ASSERT_FALSE(rlp_decode_length(buf3, sizeof(buf3), &field_length, &offset, &list));

    uint8_t buf4[] = {0xfb, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(rlp_decode_length(buf4, sizeof(buf4), &field_length, &offset, &list));
}

void test_decode_invalid_long_string_prefix_rejected(void) {
    // 0xbc..0xbf would require > 4 length bytes — the app caps at 32 bits.
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = false;
    for (uint8_t prefix = 0xbc; prefix <= 0xbf; ++prefix) {
        uint8_t buf[16] = {0};
        buf[0] = prefix;
        TEST_ASSERT_FALSE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    }
}

void test_decode_invalid_long_list_prefix_rejected(void) {
    uint32_t field_length = 0;
    uint32_t offset = 0;
    bool list = false;
    for (uint16_t prefix = 0xfc; prefix <= 0xff; ++prefix) {
        uint8_t buf[16] = {0};
        buf[0] = (uint8_t) prefix;
        TEST_ASSERT_FALSE(rlp_decode_length(buf, sizeof(buf), &field_length, &offset, &list));
    }
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_can_decode_single_byte);
    RUN_TEST(test_can_decode_short_string);
    RUN_TEST(test_can_decode_short_list);
    RUN_TEST(test_can_decode_long_string_truncated_returns_false);
    RUN_TEST(test_can_decode_long_string_complete_returns_valid);
    RUN_TEST(test_can_decode_long_string_over_4_bytes_flags_invalid);
    RUN_TEST(test_can_decode_long_list_truncated_returns_false);
    RUN_TEST(test_can_decode_long_list_over_4_bytes_flags_invalid);
    RUN_TEST(test_decode_single_byte);
    RUN_TEST(test_decode_short_string);
    RUN_TEST(test_decode_long_string_1_byte_length);
    RUN_TEST(test_decode_long_string_2_byte_length);
    RUN_TEST(test_decode_long_string_3_byte_length);
    RUN_TEST(test_decode_long_string_4_byte_length);
    RUN_TEST(test_decode_short_list);
    RUN_TEST(test_decode_long_list_1_byte_length);
    RUN_TEST(test_decode_long_list_4_byte_length);
    RUN_TEST(test_decode_empty_buffer_rejected);
    RUN_TEST(test_decode_long_string_truncated_rejected);
    RUN_TEST(test_decode_long_list_truncated_rejected);
    RUN_TEST(test_decode_invalid_long_string_prefix_rejected);
    RUN_TEST(test_decode_invalid_long_list_prefix_rejected);
    return UNITY_END();
}
