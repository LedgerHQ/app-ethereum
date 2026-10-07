/**
 * @file test_tlv_utils.c
 * @brief Unit tests for the thin TLV decoder wrappers at src/tlv_utils.c.
 *
 * Each helper applies a target-specific contract on top of the generic
 * SDK extractors in lib_tlv/tlv_library.c: NULL-output / inverted-range
 * guards up front, then forwards extraction failures, then validates
 * the decoded value against the helper's own rules (range, printable
 * ASCII, valid chain-id span).
 *
 * The test pins:
 *   - happy paths returning the decoded value through *out;
 *   - the guard rejecting a NULL output pointer (where applicable);
 *   - the min>max inverted-range guard (where applicable);
 *   - the underlying SDK extractor failure (e.g. value.size == 0
 *     or oversized) bubbling up as false;
 *   - the post-extraction validation: chain_id 0 / >MAX rejected,
 *     uint range rejected, non-printable string rejected, oversized
 *     hash rejected, address with wrong size rejected.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "tlv_utils.h"
#include "tlv_library.h"
#include "common_utils.h"

// =============================================================================
// Helpers
// =============================================================================

static tlv_data_t make_tlv(const uint8_t *bytes, uint16_t size) {
    tlv_data_t d = {0};
    d.tag = 0x42;
    // buffer_t.ptr is non-const for historical reasons even though the
    // TLV extractors only read from it — cast at the boundary.
    d.value.ptr = (uint8_t *) bytes;
    d.value.size = size;
    return d;
}

// =============================================================================
// tlv_check_challenge — mock check_challenge always returns true; we
// can only exercise the extraction-failure and happy paths.
// =============================================================================

void test_check_challenge_happy(void) {
    uint8_t bytes[4] = {0x00, 0x00, 0x00, 0x2A};  // 42
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(tlv_check_challenge(&d));
}

void test_check_challenge_extraction_fails(void) {
    // value.size == 0 → get_uint64_t_from_tlv_data rejects.
    tlv_data_t d = make_tlv(NULL, 0);
    TEST_ASSERT_FALSE(tlv_check_challenge(&d));
}

// =============================================================================
// tlv_get_chain_id
// =============================================================================

void test_get_chain_id_happy(void) {
    uint8_t bytes[1] = {137};  // Polygon
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint64_t cid = 0;
    TEST_ASSERT_TRUE(tlv_get_chain_id(&d, &cid));
    TEST_ASSERT_EQUAL(cid, 137);
}

void test_get_chain_id_null_out_rejected(void) {
    uint8_t bytes[1] = {1};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    TEST_ASSERT_FALSE(tlv_get_chain_id(&d, NULL));
}

void test_get_chain_id_extraction_fails(void) {
    tlv_data_t d = make_tlv(NULL, 0);
    uint64_t cid = 0;
    TEST_ASSERT_FALSE(tlv_get_chain_id(&d, &cid));
}

void test_get_chain_id_zero_rejected(void) {
    uint8_t bytes[1] = {0};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint64_t cid = 99;
    TEST_ASSERT_FALSE(tlv_get_chain_id(&d, &cid));
}

void test_get_chain_id_above_max_rejected(void) {
    // 0x7FFFFFFFFFFFFFFF — above MAX_VALID_CHAIN_ID = 0x7FFFFFFFFFFFFFDB
    uint8_t bytes[8] = {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint64_t cid = 0;
    TEST_ASSERT_FALSE(tlv_get_chain_id(&d, &cid));
}

// =============================================================================
// tlv_get_hash
// =============================================================================

void test_get_hash_happy(void) {
    uint8_t bytes[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t out[4] = {0};
    TEST_ASSERT_TRUE(tlv_get_hash(&d, out, sizeof(out)));
    static const uint8_t expected[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL_MEMORY(out, expected, sizeof(expected));
}

void test_get_hash_null_out_rejected(void) {
    uint8_t bytes[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    TEST_ASSERT_FALSE(tlv_get_hash(&d, NULL, 16));
}

void test_get_hash_zero_max_size_rejected(void) {
    uint8_t bytes[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t out[1] = {0};
    TEST_ASSERT_FALSE(tlv_get_hash(&d, out, 0));
}

void test_get_hash_oversize_payload_rejected(void) {
    // 5-byte payload, max_size=4 → get_buffer_from_tlv_data rejects.
    uint8_t bytes[5] = {1, 2, 3, 4, 5};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t out[4] = {0};
    TEST_ASSERT_FALSE(tlv_get_hash(&d, out, sizeof(out)));
}

// =============================================================================
// tlv_get_address
// =============================================================================

void test_get_address_happy(void) {
    uint8_t bytes[ADDRESS_LENGTH];
    memset(bytes, 0xAB, sizeof(bytes));
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t out[ADDRESS_LENGTH] = {0};
    TEST_ASSERT_TRUE(tlv_get_address(&d, out));
    TEST_ASSERT_EQUAL_MEMORY(out, bytes, ADDRESS_LENGTH);
}

void test_get_address_null_out_rejected(void) {
    uint8_t bytes[ADDRESS_LENGTH] = {0};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    TEST_ASSERT_FALSE(tlv_get_address(&d, NULL));
}

void test_get_address_wrong_size_rejected(void) {
    // 19 bytes — get_buffer_from_tlv_data with min=max=20 rejects.
    uint8_t bytes[ADDRESS_LENGTH - 1] = {0};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t out[ADDRESS_LENGTH] = {0};
    TEST_ASSERT_FALSE(tlv_get_address(&d, out));
}

// =============================================================================
// tlv_get_printable_string
// =============================================================================

void test_get_printable_string_happy(void) {
    const uint8_t bytes[] = "Hello";  // 5 chars + NUL — TLV size excludes NUL
    tlv_data_t d = make_tlv(bytes, 5);
    char out[16] = {0};
    TEST_ASSERT_TRUE(tlv_get_printable_string(&d, out, 1, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(out, "Hello");
}

void test_get_printable_string_null_out_rejected(void) {
    const uint8_t bytes[] = "Hi";
    tlv_data_t d = make_tlv(bytes, 2);
    TEST_ASSERT_FALSE(tlv_get_printable_string(&d, NULL, 0, 16));
}

void test_get_printable_string_inverted_range_rejected(void) {
    const uint8_t bytes[] = "Hi";
    tlv_data_t d = make_tlv(bytes, 2);
    char out[16] = {0};
    TEST_ASSERT_FALSE(tlv_get_printable_string(&d, out, 10, 5));
}

void test_get_printable_string_extraction_fails(void) {
    // out_size = 1 forces get_string_from_tlv_data to reject the 2-byte
    // payload (no room for NUL terminator).
    const uint8_t bytes[] = "Hi";
    tlv_data_t d = make_tlv(bytes, 2);
    char out[2] = {0};
    TEST_ASSERT_FALSE(tlv_get_printable_string(&d, out, 0, 2));
}

void test_get_printable_string_non_printable_rejected(void) {
    // Embedded control byte (0x01) — passes get_string_from_tlv_data but
    // fails is_printable_string check.
    const uint8_t bytes[] = {'H', 0x01, 'i'};
    tlv_data_t d = make_tlv(bytes, 3);
    char out[8] = {0};
    TEST_ASSERT_FALSE(tlv_get_printable_string(&d, out, 0, sizeof(out)));
}

// =============================================================================
// tlv_get_uint16_range
// =============================================================================

void test_get_uint16_range_happy(void) {
    uint8_t bytes[2] = {0x01, 0x23};  // 0x0123
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint16_t v = 0;
    TEST_ASSERT_TRUE(tlv_get_uint16_range(&d, &v, 0, UINT16_MAX));
    TEST_ASSERT_EQUAL(v, 0x0123);
}

void test_get_uint16_range_null_out_rejected(void) {
    uint8_t bytes[2] = {0, 0};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    TEST_ASSERT_FALSE(tlv_get_uint16_range(&d, NULL, 0, 100));
}

void test_get_uint16_range_inverted_rejected(void) {
    uint8_t bytes[2] = {0, 1};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint16_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint16_range(&d, &v, 100, 1));
}

void test_get_uint16_range_extraction_fails(void) {
    tlv_data_t d = make_tlv(NULL, 0);
    uint16_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint16_range(&d, &v, 0, UINT16_MAX));
}

void test_get_uint16_range_below_min_rejected(void) {
    uint8_t bytes[1] = {5};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint16_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint16_range(&d, &v, 10, 100));
}

void test_get_uint16_range_above_max_rejected(void) {
    uint8_t bytes[2] = {0x01, 0xF4};  // 500
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint16_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint16_range(&d, &v, 0, 100));
}

// =============================================================================
// tlv_get_uint8_range
// =============================================================================

void test_get_uint8_range_happy(void) {
    uint8_t bytes[1] = {42};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t v = 0;
    TEST_ASSERT_TRUE(tlv_get_uint8_range(&d, &v, 0, UINT8_MAX));
    TEST_ASSERT_EQUAL(v, 42);
}

void test_get_uint8_range_null_out_rejected(void) {
    uint8_t bytes[1] = {0};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    TEST_ASSERT_FALSE(tlv_get_uint8_range(&d, NULL, 0, 100));
}

void test_get_uint8_range_inverted_rejected(void) {
    uint8_t bytes[1] = {1};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint8_range(&d, &v, 100, 1));
}

void test_get_uint8_range_extraction_fails(void) {
    tlv_data_t d = make_tlv(NULL, 0);
    uint8_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint8_range(&d, &v, 0, UINT8_MAX));
}

void test_get_uint8_range_below_min_rejected(void) {
    uint8_t bytes[1] = {5};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint8_range(&d, &v, 10, 100));
}

void test_get_uint8_range_above_max_rejected(void) {
    uint8_t bytes[1] = {200};
    tlv_data_t d = make_tlv(bytes, sizeof(bytes));
    uint8_t v = 0;
    TEST_ASSERT_FALSE(tlv_get_uint8_range(&d, &v, 0, 100));
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
    RUN_TEST(test_check_challenge_happy);
    RUN_TEST(test_check_challenge_extraction_fails);
    RUN_TEST(test_get_chain_id_happy);
    RUN_TEST(test_get_chain_id_null_out_rejected);
    RUN_TEST(test_get_chain_id_extraction_fails);
    RUN_TEST(test_get_chain_id_zero_rejected);
    RUN_TEST(test_get_chain_id_above_max_rejected);
    RUN_TEST(test_get_hash_happy);
    RUN_TEST(test_get_hash_null_out_rejected);
    RUN_TEST(test_get_hash_zero_max_size_rejected);
    RUN_TEST(test_get_hash_oversize_payload_rejected);
    RUN_TEST(test_get_address_happy);
    RUN_TEST(test_get_address_null_out_rejected);
    RUN_TEST(test_get_address_wrong_size_rejected);
    RUN_TEST(test_get_printable_string_happy);
    RUN_TEST(test_get_printable_string_null_out_rejected);
    RUN_TEST(test_get_printable_string_inverted_range_rejected);
    RUN_TEST(test_get_printable_string_extraction_fails);
    RUN_TEST(test_get_printable_string_non_printable_rejected);
    RUN_TEST(test_get_uint16_range_happy);
    RUN_TEST(test_get_uint16_range_null_out_rejected);
    RUN_TEST(test_get_uint16_range_inverted_rejected);
    RUN_TEST(test_get_uint16_range_extraction_fails);
    RUN_TEST(test_get_uint16_range_below_min_rejected);
    RUN_TEST(test_get_uint16_range_above_max_rejected);
    RUN_TEST(test_get_uint8_range_happy);
    RUN_TEST(test_get_uint8_range_null_out_rejected);
    RUN_TEST(test_get_uint8_range_inverted_rejected);
    RUN_TEST(test_get_uint8_range_extraction_fails);
    RUN_TEST(test_get_uint8_range_below_min_rejected);
    RUN_TEST(test_get_uint8_range_above_max_rejected);
    return UNITY_END();
}
