/**
 * @file test_cmd_safe_account.c
 * @brief Unit tests for handle_safe_account / clear_safe_account at
 *        src/features/provide_safe_account/cmd_safe_account.c.
 *
 * The Safe Account APDU sequence is the host's way of saying "this
 * transaction is a Gnosis Safe execTransaction; here is the Safe
 * descriptor and the signer list". It's a small state machine:
 *
 *   1. P2 = SAFE_DESCRIPTOR (0x00): first APDU sets SAFE_DESC. Refused if
 *      SAFE_DESC already exists (anti-replay across the same review).
 *   2. P2 = SIGNER_DESCRIPTOR (0x01): subsequent APDUs set the signer list.
 *      Refused if SAFE_DESC is missing (state mismatch) or if
 *      SIGNER_DESC.data is already populated (one set only).
 *   3. When SIGNER_DESC.is_valid flips true after parsing, the review
 *      screen is shown synchronously and the handler returns
 *      SWO_NO_RESPONSE (the UI thread will reply later).
 *
 * Pin every reject / accept branch plus the small clear_safe_account
 * helper. The TLV parsing itself is pinned by test_safe_descriptors.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "Mocktlv_apdu.h"
#include "cmd_safe_account.h"
#include "safe_descriptor.h"
#include "signer_descriptor.h"
// =============================================================================
// Globals the unit under test reads
// =============================================================================
// SAFE_DESC and SIGNER_DESC are declared `extern` in safe_descriptor.h /
// signer_descriptor.h. We're not linking the real .c files, so provide
// storage here.
safe_descriptor_t *SAFE_DESC;
signers_descriptor_t SIGNER_DESC;

static bool g_handle_safe_tlv_payload_ret = true;
static bool g_handle_signer_tlv_payload_ret = true;

// =============================================================================
// Wraps
// =============================================================================

bool handle_safe_tlv_payload(const buffer_t *payload) {
    (void) payload;
    return (bool) g_handle_safe_tlv_payload_ret;
}

bool handle_signer_tlv_payload(const buffer_t *payload) {
    (void) payload;
    return (bool) g_handle_signer_tlv_payload_ret;
}

static int g_ui_display_calls = 0;
void ui_display_safe_account(void) {
    g_ui_display_calls++;
}

static int g_clear_safe_calls = 0;
void clear_safe_descriptor(void) {
    g_clear_safe_calls++;
    SAFE_DESC = NULL;
}

static int g_clear_signer_calls = 0;
void clear_signer_descriptor(void) {
    g_clear_signer_calls++;
    memset(&SIGNER_DESC, 0, sizeof(SIGNER_DESC));
}

// =============================================================================
// Local tlv_from_apdu stub
// =============================================================================

static bool s_tlv_first_chunk = false;
static uint8_t s_tlv_lc = 0;
static void *s_tlv_handler = NULL;
static e_tlv_apdu_ret s_tlv_ret = TLV_APDU_SUCCESS;

static e_tlv_apdu_ret tlv_from_apdu_stub(bool first_chunk,
                                         uint8_t lc,
                                         const uint8_t *payload,
                                         f_tlv_payload_handler handler,
                                         int cmock_num_calls) {
    (void) payload;
    (void) cmock_num_calls;
    s_tlv_first_chunk = first_chunk;
    s_tlv_lc = lc;
    union {
        f_tlv_payload_handler fn;
        void *ptr;
    } u = {.fn = handler};
    s_tlv_handler = u.ptr;
    return s_tlv_ret;
}

// =============================================================================
// Fixture
// =============================================================================

// Backing storage for the SAFE_DESC pointer when a test wants to simulate
// "already-existing descriptor".
static safe_descriptor_t s_safe_storage;

static void reset(void) {
    SAFE_DESC = NULL;
    memset(&SIGNER_DESC, 0, sizeof(SIGNER_DESC));
    memset(&s_safe_storage, 0, sizeof(s_safe_storage));
    s_tlv_first_chunk = false;
    s_tlv_lc = 0;
    s_tlv_handler = NULL;
    s_tlv_ret = TLV_APDU_SUCCESS;
    g_ui_display_calls = 0;
    g_clear_safe_calls = 0;
    g_clear_signer_calls = 0;
}

// =============================================================================
// p1 / p2 sanity rejects (no TLV path entered)
// =============================================================================

void test_invalid_p1_rejected(void) {
    // tlv_from_apdu MUST NOT be reached -- the queue is intentionally empty.
    uint16_t sw = handle_safe_account(/*p1*/ 0xFF, /*p2*/ 0, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

void test_invalid_p2_rejected(void) {
    uint16_t sw = handle_safe_account(P1_FIRST_CHUNK, /*p2*/ 0xEE, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

// =============================================================================
// State-machine rejects -- order-of-operations matters
// =============================================================================

void test_safe_already_exists_rejected(void) {
    // SAFE_DESC is already populated -- re-posting a SAFE_DESCRIPTOR APDU
    // would silently replace it. Refuse.
    SAFE_DESC = &s_safe_storage;
    uint16_t sw = handle_safe_account(P1_FIRST_CHUNK, /*p2*/ 0, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_FILE_ALREADY_EXISTS);
}

void test_signer_without_safe_rejected(void) {
    // Posting SIGNER_DESCRIPTOR before SAFE_DESCRIPTOR is a state-machine
    // violation -- without a Safe descriptor we have no contract address
    // to bind the signers to.
    SAFE_DESC = NULL;
    uint16_t sw = handle_safe_account(P1_FIRST_CHUNK, /*p2*/ 1, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_COMMAND_NOT_ALLOWED);
}

void test_signer_already_exists_rejected(void) {
    SAFE_DESC = &s_safe_storage;
    // Mark SIGNER_DESC.data as already populated. Any non-NULL value
    // triggers the guard.
    static signer_data_t fake;
    SIGNER_DESC.data = &fake;
    uint16_t sw = handle_safe_account(P1_FIRST_CHUNK, /*p2*/ 1, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_FILE_ALREADY_EXISTS);
}

// =============================================================================
// Happy / sad TLV paths
// =============================================================================

void test_safe_descriptor_tlv_failure_returns_incorrect_data(void) {
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_safe_account(P1_FIRST_CHUNK, /*p2*/ 0, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_TRUE(s_tlv_first_chunk);
    // Function-pointer <-> void* cast is UB per ISO C but defined on
    // every POSIX platform; the union sidesteps -Wpedantic.
    union {
        f_tlv_payload_handler fn;
        void *ptr;
    } u = {.fn = &handle_safe_tlv_payload};
    TEST_ASSERT_EQUAL_PTR(s_tlv_handler, u.ptr);
}

void test_safe_descriptor_tlv_success_returns_success(void) {
    // First chunk of a SAFE_DESCRIPTOR; tlv_from_apdu accepts it but
    // SIGNER_DESC.is_valid stays false (no signer posted yet), so the
    // handler returns SWO_SUCCESS without showing the UI.
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_safe_account(P1_FIRST_CHUNK, /*p2*/ 0, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_ui_display_calls, 0);
}

void test_signer_descriptor_tlv_failure_returns_incorrect_data(void) {
    SAFE_DESC = &s_safe_storage;
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_safe_account(P1_FOLLOWING_CHUNK, /*p2*/ 1, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_FALSE(s_tlv_first_chunk);
    union {
        f_tlv_payload_handler fn;
        void *ptr;
    } u = {.fn = &handle_signer_tlv_payload};
    TEST_ASSERT_EQUAL_PTR(s_tlv_handler, u.ptr);
}

void test_signer_complete_triggers_ui_and_no_response(void) {
    // SAFE_DESC posted; SIGNER_DESC.data is NULL (not yet posted); after
    // tlv_from_apdu the parser flips SIGNER_DESC.is_valid true (last
    // chunk arrived). The dispatcher MUST show the UI synchronously and
    // report SWO_NO_RESPONSE so the foreground waits for user confirmation.
    SAFE_DESC = &s_safe_storage;
    s_tlv_ret = TLV_APDU_SUCCESS;
    // Simulate the parser flipping is_valid by post-hooking the mock.
    SIGNER_DESC.is_valid = true;
    uint16_t sw = handle_safe_account(P1_FOLLOWING_CHUNK, /*p2*/ 1, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_ui_display_calls, 1);
}

void test_signer_incomplete_still_returns_success_no_ui(void) {
    // Multi-chunk SIGNER stream: a non-last chunk parses successfully but
    // SIGNER_DESC.is_valid stays false -- the dispatcher reports SWO_SUCCESS
    // and the host keeps streaming.
    SAFE_DESC = &s_safe_storage;
    s_tlv_ret = TLV_APDU_SUCCESS;
    SIGNER_DESC.is_valid = false;
    uint16_t sw = handle_safe_account(P1_FOLLOWING_CHUNK, /*p2*/ 1, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_ui_display_calls, 0);
}

// =============================================================================
// clear_safe_account -- bundles the two underlying clears
// =============================================================================

void test_clear_safe_account_clears_both(void) {
    clear_safe_account();
    TEST_ASSERT_EQUAL(g_clear_safe_calls, 1);
    TEST_ASSERT_EQUAL(g_clear_signer_calls, 1);
}

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
    RUN_TEST(test_invalid_p1_rejected);
    RUN_TEST(test_invalid_p2_rejected);
    RUN_TEST(test_safe_already_exists_rejected);
    RUN_TEST(test_signer_without_safe_rejected);
    RUN_TEST(test_signer_already_exists_rejected);
    RUN_TEST(test_safe_descriptor_tlv_failure_returns_incorrect_data);
    RUN_TEST(test_safe_descriptor_tlv_success_returns_success);
    RUN_TEST(test_signer_descriptor_tlv_failure_returns_incorrect_data);
    RUN_TEST(test_signer_complete_triggers_ui_and_no_response);
    RUN_TEST(test_signer_incomplete_still_returns_success_no_ui);
    RUN_TEST(test_clear_safe_account_clears_both);
    return UNITY_END();
}
