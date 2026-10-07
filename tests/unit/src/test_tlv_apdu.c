/**
 * @file test_tlv_apdu.c
 * @brief Unit tests for the TLV-over-APDU streamer at src/tlv_apdu.c.
 *
 * tlv_from_apdu() reassembles a multi-chunk TLV payload sent by the
 * host across multiple INS_PROVIDE_* APDUs. The first chunk carries a
 * big-endian uint16 length prefix; subsequent chunks just carry data.
 * When the payload is short enough to fit in the first chunk, no heap
 * allocation happens — the callback runs against the APDU buffer
 * directly. Otherwise a temporary buffer is allocated and freed once
 * the full payload has been reassembled and the callback returned.
 *
 * Tests pin:
 *   - the parameter guards (NULL payload / NULL handler),
 *   - the first-chunk shape: length prefix < lc, header reserved,
 *   - the "incomplete previous payload" defense (calling first_chunk
 *     twice without finishing the first session),
 *   - the handler-mismatch defense on follow-up chunks,
 *   - empty payload rejection,
 *   - the single-chunk happy path,
 *   - the multi-chunk happy path: first + N follow-up chunks,
 *   - oversize chunk rejection (payload longer than declared size),
 *   - handler-returning-false propagation as TLV_APDU_ERROR.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "tlv_apdu.h"

// =============================================================================
// Handler fixture
// =============================================================================

static int g_handler_calls = 0;
static bool g_handler_return = true;
static uint8_t g_handler_seen_bytes[256];
static size_t g_handler_seen_size = 0;

static bool fake_handler(const buffer_t *buf) {
    g_handler_calls++;
    if (buf->size <= sizeof(g_handler_seen_bytes)) {
        memcpy(g_handler_seen_bytes, buf->ptr, buf->size);
        g_handler_seen_size = buf->size;
    }
    return g_handler_return;
}

static bool other_handler(const buffer_t *buf) {
    (void) buf;
    return true;
}

static void reset(void) {
    // Reset internal tlv_apdu state by calling with NULL inputs.
    // The NULL-payload guard runs reset_state() internally.
    tlv_from_apdu(false, 0, NULL, NULL);
    g_handler_calls = 0;
    g_handler_return = true;
    g_handler_seen_size = 0;
    memset(g_handler_seen_bytes, 0, sizeof(g_handler_seen_bytes));
}

// =============================================================================
// Guards
// =============================================================================

void test_null_payload_rejected(void) {
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, 5, NULL, &fake_handler), TLV_APDU_ERROR);
}

void test_null_handler_rejected(void) {
    uint8_t payload[3] = {0};
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, 3, payload, NULL), TLV_APDU_ERROR);
}

void test_first_chunk_too_short_for_length_prefix(void) {
    uint8_t payload[1] = {0};
    // lc=1 but the size prefix needs 2 bytes.
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, 1, payload, &fake_handler), TLV_APDU_ERROR);
}

void test_first_chunk_while_session_in_progress_rejected(void) {
    // First chunk declares size=10 — multi-chunk reassembly starts.
    uint8_t first[] = {0x00, 0x0A, 0x11};  // size=10, 1 byte of data
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(first), first, &fake_handler), TLV_APDU_PENDING);
    // Sending another first_chunk now must fail — the previous session
    // is still in progress.
    uint8_t replay[] = {0x00, 0x02, 0x22, 0x33};
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(replay), replay, &fake_handler), TLV_APDU_ERROR);
}

void test_followup_chunk_handler_mismatch_rejected(void) {
    uint8_t first[] = {0x00, 0x0A, 0x11};
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(first), first, &fake_handler), TLV_APDU_PENDING);
    // Use a different handler on the follow-up chunk → must reject.
    uint8_t followup[2] = {0x22, 0x33};
    TEST_ASSERT_EQUAL(tlv_from_apdu(false, sizeof(followup), followup, &other_handler),
                      TLV_APDU_ERROR);
}

void test_zero_length_payload_rejected(void) {
    uint8_t bytes[] = {0x00, 0x00};  // size = 0
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(bytes), bytes, &fake_handler), TLV_APDU_ERROR);
}

void test_chunk_overflows_declared_size(void) {
    // Declared size = 2, but the first chunk carries 5 data bytes.
    uint8_t bytes[] = {0x00, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(bytes), bytes, &fake_handler), TLV_APDU_ERROR);
}

// =============================================================================
// Happy paths
// =============================================================================

void test_single_chunk_invokes_handler_with_apdu_buffer(void) {
    // Declared size 3, data {0xAA, 0xBB, 0xCC} fits entirely in this chunk.
    uint8_t bytes[] = {0x00, 0x03, 0xAA, 0xBB, 0xCC};
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(bytes), bytes, &fake_handler), TLV_APDU_SUCCESS);
    TEST_ASSERT_EQUAL(g_handler_calls, 1);
    TEST_ASSERT_EQUAL(g_handler_seen_size, 3);
    static const uint8_t expected[] = {0xAA, 0xBB, 0xCC};
    TEST_ASSERT_EQUAL_MEMORY(g_handler_seen_bytes, expected, 3);
}

void test_multi_chunk_reassembles_and_invokes_handler(void) {
    // size=5, split across two chunks: first carries 2 bytes, second
    // carries 3.
    uint8_t first[] = {0x00, 0x05, 0xAA, 0xBB};
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(first), first, &fake_handler), TLV_APDU_PENDING);
    TEST_ASSERT_EQUAL(g_handler_calls, 0);

    uint8_t second[] = {0xCC, 0xDD, 0xEE};
    TEST_ASSERT_EQUAL(tlv_from_apdu(false, sizeof(second), second, &fake_handler),
                      TLV_APDU_SUCCESS);
    TEST_ASSERT_EQUAL(g_handler_calls, 1);
    TEST_ASSERT_EQUAL(g_handler_seen_size, 5);
    static const uint8_t expected[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    TEST_ASSERT_EQUAL_MEMORY(g_handler_seen_bytes, expected, 5);
}

void test_handler_failure_propagates_as_error(void) {
    uint8_t bytes[] = {0x00, 0x02, 0xAA, 0xBB};
    g_handler_return = false;
    TEST_ASSERT_EQUAL(tlv_from_apdu(true, sizeof(bytes), bytes, &fake_handler), TLV_APDU_ERROR);
    TEST_ASSERT_EQUAL(g_handler_calls, 1);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_null_payload_rejected);
    RUN_TEST(test_null_handler_rejected);
    RUN_TEST(test_first_chunk_too_short_for_length_prefix);
    RUN_TEST(test_first_chunk_while_session_in_progress_rejected);
    RUN_TEST(test_followup_chunk_handler_mismatch_rejected);
    RUN_TEST(test_zero_length_payload_rejected);
    RUN_TEST(test_chunk_overflows_declared_size);
    RUN_TEST(test_single_chunk_invokes_handler_with_apdu_buffer);
    RUN_TEST(test_multi_chunk_reassembles_and_invokes_handler);
    RUN_TEST(test_handler_failure_propagates_as_error);
    return UNITY_END();
}
