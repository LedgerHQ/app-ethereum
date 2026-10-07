/**
 * @file test_calldata.c
 * @brief Unit tests for the chunked-calldata buffer at
 *        src/features/generic_tx_parser/calldata.c.
 *
 * The calldata buffer is the storage backing the GCS / EIP-712 data
 * paths: the host streams ABI-encoded calldata in arbitrary-size
 * chunks, the device groups them into 32-byte chunks, compresses each
 * chunk by stripping trailing-or-leading zeros (whichever side wins),
 * and exposes a chunk-indexed read API.
 *
 * Behaviors covered:
 *   - lifecycle: calldata_init / calldata_set_selector / calldata_delete,
 *   - append discipline: overflow rejection on expected_size, byte-
 *     stream split across multiple appends still produces the correct
 *     chunks,
 *   - completeness gate: get_selector / get_chunk both return NULL
 *     while received_size < expected_size,
 *   - chunk indexing: idx beyond the list returns NULL,
 *   - compression round-trip: STRIP_LEFT (zero-prefixed values like
 *     uint256(1)) and STRIP_RIGHT (zero-suffixed values like a 4-byte
 *     selector padded with zeros) both reproduce the original chunk
 *     byte-for-byte through decompress_chunk.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "calldata.h"

// =============================================================================
// Fixtures
// =============================================================================

static const uint8_t g_selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

// =============================================================================
// calldata_init
// =============================================================================

void test_init_with_selector(void) {
    s_calldata *cd = calldata_init(64, g_selector);
    TEST_ASSERT_NOT_NULL(cd);
    TEST_ASSERT_EQUAL(cd->expected_size, 64);
    TEST_ASSERT_EQUAL(cd->received_size, 0);
    TEST_ASSERT_EQUAL_MEMORY(cd->selector, g_selector, CALLDATA_SELECTOR_SIZE);
    calldata_delete(cd);
}

void test_init_with_null_selector(void) {
    s_calldata *cd = calldata_init(32, NULL);
    TEST_ASSERT_NOT_NULL(cd);
    TEST_ASSERT_EQUAL(cd->expected_size, 32);
    // calloc-zeroed selector
    static const uint8_t zero[CALLDATA_SELECTOR_SIZE] = {0};
    TEST_ASSERT_EQUAL_MEMORY(cd->selector, zero, CALLDATA_SELECTOR_SIZE);
    calldata_delete(cd);
}

// =============================================================================
// calldata_set_selector
// =============================================================================

void test_set_selector_happy_path(void) {
    s_calldata *cd = calldata_init(32, NULL);
    TEST_ASSERT_NOT_NULL(cd);
    TEST_ASSERT_TRUE(calldata_set_selector(cd, g_selector));
    TEST_ASSERT_EQUAL_MEMORY(cd->selector, g_selector, CALLDATA_SELECTOR_SIZE);
    calldata_delete(cd);
}

void test_set_selector_null_calldata_rejected(void) {
    TEST_ASSERT_FALSE(calldata_set_selector(NULL, g_selector));
}

void test_set_selector_null_selector_rejected(void) {
    s_calldata *cd = calldata_init(32, NULL);
    TEST_ASSERT_FALSE(calldata_set_selector(cd, NULL));
    calldata_delete(cd);
}

// =============================================================================
// calldata_append — overflow & happy-path appends
// =============================================================================

void test_append_one_chunk_all_at_once(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    static const uint8_t chunk[CALLDATA_CHUNK_SIZE] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    };
    TEST_ASSERT_TRUE(calldata_append(cd, chunk, sizeof(chunk)));
    TEST_ASSERT_EQUAL(cd->received_size, CALLDATA_CHUNK_SIZE);
    calldata_delete(cd);
}

void test_append_byte_by_byte_assembles_chunk(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    static const uint8_t chunk[CALLDATA_CHUNK_SIZE] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
    };
    for (size_t i = 0; i < sizeof(chunk); ++i) {
        TEST_ASSERT_TRUE(calldata_append(cd, &chunk[i], 1));
    }
    TEST_ASSERT_EQUAL(cd->received_size, CALLDATA_CHUNK_SIZE);

    // After completion, get_chunk(0) must return the original bytes.
    const uint8_t *read = calldata_get_chunk(cd, 0);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_MEMORY(read, chunk, CALLDATA_CHUNK_SIZE);
    calldata_delete(cd);
}

void test_append_overflow_total_rejected(void) {
    s_calldata *cd = calldata_init(10, NULL);
    uint8_t buf[20] = {0};
    // 20 > expected 10 → rejected with no partial state
    TEST_ASSERT_FALSE(calldata_append(cd, buf, 20));
    TEST_ASSERT_EQUAL(cd->received_size, 0);
    calldata_delete(cd);
}

void test_append_overflow_cumulative_rejected(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    uint8_t buf[CALLDATA_CHUNK_SIZE - 5] = {0};
    TEST_ASSERT_TRUE(calldata_append(cd, buf, sizeof(buf)));
    // After 27 bytes, expected_size - received_size = 5. Trying to push
    // 10 more must be rejected.
    uint8_t extra[10] = {0};
    TEST_ASSERT_FALSE(calldata_append(cd, extra, sizeof(extra)));
    calldata_delete(cd);
}

void test_append_null_calldata_rejected(void) {
    uint8_t buf[1] = {0};
    TEST_ASSERT_FALSE(calldata_append(NULL, buf, 1));
}

// =============================================================================
// calldata_get_selector — gated on completeness
// =============================================================================

void test_get_selector_incomplete_returns_null(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, g_selector);
    // received_size < expected_size at this point
    TEST_ASSERT_NULL(calldata_get_selector(cd));
    calldata_delete(cd);
}

void test_get_selector_complete_returns_selector(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, g_selector);
    uint8_t buf[CALLDATA_CHUNK_SIZE] = {0};
    TEST_ASSERT_TRUE(calldata_append(cd, buf, CALLDATA_CHUNK_SIZE));
    const uint8_t *sel = calldata_get_selector(cd);
    TEST_ASSERT_NOT_NULL(sel);
    TEST_ASSERT_EQUAL_MEMORY(sel, g_selector, CALLDATA_SELECTOR_SIZE);
    calldata_delete(cd);
}

void test_get_selector_null_calldata_returns_null(void) {
    TEST_ASSERT_NULL(calldata_get_selector(NULL));
}

// =============================================================================
// calldata_get_chunk — index + completeness
// =============================================================================

void test_get_chunk_incomplete_returns_null(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    TEST_ASSERT_NULL(calldata_get_chunk(cd, 0));
    calldata_delete(cd);
}

void test_get_chunk_idx_out_of_range_returns_null(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    uint8_t buf[CALLDATA_CHUNK_SIZE] = {0};
    TEST_ASSERT_TRUE(calldata_append(cd, buf, CALLDATA_CHUNK_SIZE));
    // One chunk exists at idx 0; idx 1 must return NULL.
    TEST_ASSERT_NOT_NULL(calldata_get_chunk(cd, 0));
    TEST_ASSERT_NULL(calldata_get_chunk(cd, 1));
    calldata_delete(cd);
}

void test_get_chunk_zero_expected_size_returns_null(void) {
    // expected_size == 0 → no chunks ever appended → get_chunk(0) NULL.
    s_calldata *cd = calldata_init(0, NULL);
    TEST_ASSERT_NULL(calldata_get_chunk(cd, 0));
    calldata_delete(cd);
}

void test_get_chunk_multi_chunk_walks_list(void) {
    // Two chunks: first all zeros, second has data at the end.
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE * 2, NULL);
    uint8_t chunk_a[CALLDATA_CHUNK_SIZE] = {0};
    uint8_t chunk_b[CALLDATA_CHUNK_SIZE] = {0};
    chunk_b[31] = 0x42;
    TEST_ASSERT_TRUE(calldata_append(cd, chunk_a, CALLDATA_CHUNK_SIZE));
    TEST_ASSERT_TRUE(calldata_append(cd, chunk_b, CALLDATA_CHUNK_SIZE));

    const uint8_t *read_a = calldata_get_chunk(cd, 0);
    TEST_ASSERT_NOT_NULL(read_a);
    TEST_ASSERT_EQUAL_MEMORY(read_a, chunk_a, CALLDATA_CHUNK_SIZE);

    const uint8_t *read_b = calldata_get_chunk(cd, 1);
    TEST_ASSERT_NOT_NULL(read_b);
    TEST_ASSERT_EQUAL_MEMORY(read_b, chunk_b, CALLDATA_CHUNK_SIZE);
    calldata_delete(cd);
}

// =============================================================================
// Compression round-trip — STRIP_LEFT vs STRIP_RIGHT
// =============================================================================

void test_compression_round_trip_strip_left(void) {
    // Lots of leading zeros (uint256 small value), one byte of payload at end.
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    uint8_t chunk[CALLDATA_CHUNK_SIZE] = {0};
    chunk[31] = 0xAB;
    TEST_ASSERT_TRUE(calldata_append(cd, chunk, sizeof(chunk)));
    const uint8_t *read = calldata_get_chunk(cd, 0);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_MEMORY(read, chunk, sizeof(chunk));
    calldata_delete(cd);
}

void test_compression_round_trip_strip_right(void) {
    // Lots of trailing zeros, payload at the start (e.g. selector pattern).
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    uint8_t chunk[CALLDATA_CHUNK_SIZE] = {0};
    chunk[0] = 0xCA;
    chunk[1] = 0xFE;
    chunk[2] = 0xBA;
    chunk[3] = 0xBE;
    TEST_ASSERT_TRUE(calldata_append(cd, chunk, sizeof(chunk)));
    const uint8_t *read = calldata_get_chunk(cd, 0);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_MEMORY(read, chunk, sizeof(chunk));
    calldata_delete(cd);
}

void test_compression_round_trip_all_zero(void) {
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    uint8_t chunk[CALLDATA_CHUNK_SIZE] = {0};  // all zeros
    TEST_ASSERT_TRUE(calldata_append(cd, chunk, sizeof(chunk)));
    const uint8_t *read = calldata_get_chunk(cd, 0);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_MEMORY(read, chunk, sizeof(chunk));
    calldata_delete(cd);
}

void test_compression_round_trip_no_zeros(void) {
    // Chunk with no zero bytes — both strip directions yield size 32.
    s_calldata *cd = calldata_init(CALLDATA_CHUNK_SIZE, NULL);
    uint8_t chunk[CALLDATA_CHUNK_SIZE];
    for (size_t i = 0; i < sizeof(chunk); ++i) chunk[i] = (uint8_t) (i + 1);
    TEST_ASSERT_TRUE(calldata_append(cd, chunk, sizeof(chunk)));
    const uint8_t *read = calldata_get_chunk(cd, 0);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_MEMORY(read, chunk, sizeof(chunk));
    calldata_delete(cd);
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
    RUN_TEST(test_init_with_selector);
    RUN_TEST(test_init_with_null_selector);
    RUN_TEST(test_set_selector_happy_path);
    RUN_TEST(test_set_selector_null_calldata_rejected);
    RUN_TEST(test_set_selector_null_selector_rejected);
    RUN_TEST(test_append_one_chunk_all_at_once);
    RUN_TEST(test_append_byte_by_byte_assembles_chunk);
    RUN_TEST(test_append_overflow_total_rejected);
    RUN_TEST(test_append_overflow_cumulative_rejected);
    RUN_TEST(test_append_null_calldata_rejected);
    RUN_TEST(test_get_selector_incomplete_returns_null);
    RUN_TEST(test_get_selector_complete_returns_selector);
    RUN_TEST(test_get_selector_null_calldata_returns_null);
    RUN_TEST(test_get_chunk_incomplete_returns_null);
    RUN_TEST(test_get_chunk_idx_out_of_range_returns_null);
    RUN_TEST(test_get_chunk_zero_expected_size_returns_null);
    RUN_TEST(test_get_chunk_multi_chunk_walks_list);
    RUN_TEST(test_compression_round_trip_strip_left);
    RUN_TEST(test_compression_round_trip_strip_right);
    RUN_TEST(test_compression_round_trip_all_zero);
    RUN_TEST(test_compression_round_trip_no_zeros);
    return UNITY_END();
}
