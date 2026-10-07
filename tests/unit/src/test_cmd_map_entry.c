/**
 * @file test_cmd_map_entry.c
 * @brief Unit tests for the INS_PROVIDE_MAP_ENTRY APDU entry point at
 *        src/features/provide_map_entry/cmd_map_entry.c.
 *
 * The deep parsing path (handle_map_entry_tlv_payload +
 * verify_map_entry_struct) is already covered by test_provide_map_entry;
 * this slice pins the entry-point guards added by commit 092cba0c:
 *   - P1 must be either P1_FIRST_CHUNK or P1_FOLLOWING_CHUNK,
 *     anything else returns SWO_WRONG_P1_P2,
 *   - P2 must be 0, anything else returns SWO_WRONG_P1_P2,
 * plus the standard tlv_from_apdu success / failure mapping to
 * SWO_SUCCESS / SWO_INCORRECT_DATA.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmd_map_entry.h"
#include "apdu_constants.h"
#include "status_words.h"
#include "Mocktlv_apdu.h"

// =============================================================================
// Wraps
// =============================================================================

static e_tlv_apdu_ret g_tlv_ret = TLV_APDU_SUCCESS;
static bool g_invoke_handler = false;
static bool g_handler_returned = false;
static e_tlv_apdu_ret tlv_from_apdu_stub(bool first_chunk,
                                         uint8_t lc,
                                         const uint8_t *payload,
                                         f_tlv_payload_handler handler,
                                         int cmock_num_calls) {
    (void) first_chunk;
    (void) lc;
    (void) payload;
    (void) cmock_num_calls;
    if (g_invoke_handler && handler != NULL) {
        buffer_t buf = {.ptr = NULL, .size = 0, .offset = 0};
        g_handler_returned = handler(&buf);
    }
    return g_tlv_ret;
}

// The static handle_tlv_payload helpers reference these symbols; the
// invoke-handler tests below drive their return values to exercise
// each branch in cmd_map_entry's static handle_tlv_payload.
static bool g_handle_payload_ret = true;
static bool g_verify_ret = true;
bool handle_map_entry_tlv_payload(const void *buf, void *ctx) {
    (void) buf;
    (void) ctx;
    return g_handle_payload_ret;
}
bool verify_map_entry_struct(const void *ctx) {
    (void) ctx;
    return g_verify_ret;
}
// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    g_tlv_ret = TLV_APDU_SUCCESS;
    g_invoke_handler = false;
    g_handler_returned = false;
    g_handle_payload_ret = true;
    g_verify_ret = true;
}

// =============================================================================
// P1 validation (commit 092cba0c)
// =============================================================================

void test_p1_first_chunk_accepted(void) {
    TEST_ASSERT_EQUAL(handle_map_entry(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

void test_p1_following_chunk_accepted(void) {
    TEST_ASSERT_EQUAL(handle_map_entry(P1_FOLLOWING_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

void test_p1_reserved_value_rejected(void) {
    // 0x02 is reserved (only 0x00 = FOLLOWING and 0x01 = FIRST are valid).
    TEST_ASSERT_EQUAL(handle_map_entry(0x02, 0, 0, NULL), SWO_WRONG_P1_P2);
}

void test_p1_junk_value_rejected(void) {
    TEST_ASSERT_EQUAL(handle_map_entry(0x42, 0, 0, NULL), SWO_WRONG_P1_P2);
    TEST_ASSERT_EQUAL(handle_map_entry(0xFF, 0, 0, NULL), SWO_WRONG_P1_P2);
}

// =============================================================================
// P2 validation (commit 092cba0c)
// =============================================================================

void test_p2_nonzero_rejected_even_with_valid_p1(void) {
    // P1 is valid but P2 is non-zero — must still reject.
    TEST_ASSERT_EQUAL(handle_map_entry(P1_FIRST_CHUNK, 0x01, 0, NULL), SWO_WRONG_P1_P2);
    TEST_ASSERT_EQUAL(handle_map_entry(P1_FOLLOWING_CHUNK, 0xFF, 0, NULL), SWO_WRONG_P1_P2);
}

// =============================================================================
// tlv_from_apdu integration
// =============================================================================

void test_tlv_failure_returns_incorrect_data(void) {
    g_tlv_ret = TLV_APDU_ERROR;
    TEST_ASSERT_EQUAL(handle_map_entry(P1_FIRST_CHUNK, 0, 0, NULL), SWO_INCORRECT_DATA);
}

void test_tlv_success_returns_success(void) {
    g_tlv_ret = TLV_APDU_SUCCESS;
    TEST_ASSERT_EQUAL(handle_map_entry(P1_FOLLOWING_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

// =============================================================================
// Internal handle_tlv_payload — exercised through the wrap
// =============================================================================

void test_handler_happy_path(void) {
    g_invoke_handler = true;
    handle_map_entry(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_TRUE(g_handler_returned);
}

void test_handler_payload_failure(void) {
    g_invoke_handler = true;
    g_handle_payload_ret = false;
    handle_map_entry(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_handler_returned);
}

void test_handler_verify_failure(void) {
    g_invoke_handler = true;
    g_verify_ret = false;
    handle_map_entry(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_handler_returned);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocktlv_apdu_Init();
    tlv_from_apdu_StubWithCallback(tlv_from_apdu_stub);
    reset();
}
void tearDown(void) {
    Mocktlv_apdu_Verify();
    Mocktlv_apdu_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_p1_first_chunk_accepted);
    RUN_TEST(test_p1_following_chunk_accepted);
    RUN_TEST(test_p1_reserved_value_rejected);
    RUN_TEST(test_p1_junk_value_rejected);
    RUN_TEST(test_p2_nonzero_rejected_even_with_valid_p1);
    RUN_TEST(test_tlv_failure_returns_incorrect_data);
    RUN_TEST(test_tlv_success_returns_success);
    RUN_TEST(test_handler_happy_path);
    RUN_TEST(test_handler_payload_failure);
    RUN_TEST(test_handler_verify_failure);
    return UNITY_END();
}
