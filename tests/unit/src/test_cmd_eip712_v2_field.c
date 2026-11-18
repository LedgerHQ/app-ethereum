/**
 * @file test_cmd_eip712_v2_field.c
 * @brief Unit tests for the FIELD command of an EIP-712 V2 message at
 *        src/features/sign_message_eip712/cmd_eip712_v2_field.c
 *
 * The command takes the FIELD descriptors a message info announces, between that message info
 * and the review. It only goes on in a V2 session, in the state the values left the app in, and
 * hands each complete descriptor to the shared FIELD handler.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmd_eip712_v2_field.h"
#include "eip712_session.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "status_words.h"
#include "Mocktlv_apdu.h"

// =============================================================================
// Stubs
// =============================================================================

static bool g_fields_pending = true;
bool operation_ctx_message_has_pending_fields(void) {
    return g_fields_pending;
}

// the shared handler: it is not what these tests are about
static bool g_field_ret = true;
static int g_field_calls = 0;
bool handle_field_tlv_payload(const buffer_t *buf) {
    (void) buf;
    g_field_calls++;
    return g_field_ret;
}

// tlv_from_apdu: the stub runs the handler the command gave, as if the payload were complete
static e_tlv_apdu_ret g_tlv_ret = TLV_APDU_SUCCESS;
static bool g_tlv_run_handler = true;
static int g_tlv_calls = 0;
static e_tlv_apdu_ret tlv_from_apdu_stub(bool first_chunk,
                                         uint8_t lc,
                                         const uint8_t *payload,
                                         f_tlv_payload_handler handler,
                                         int cmock_num_calls) {
    buffer_t buf = {0};

    (void) first_chunk;
    (void) lc;
    (void) payload;
    (void) cmock_num_calls;
    g_tlv_calls++;
    if (g_tlv_run_handler && !handler(&buf)) {
        return TLV_APDU_ERROR;
    }
    return g_tlv_ret;
}

static uint16_t send(uint8_t p1) {
    return handle_eip712_v2_field(p1, P2_EIP712_V2_IMPLEM, 0, NULL);
}

// =============================================================================
// Tests
// =============================================================================

void test_a_descriptor_is_handed_to_the_field_handler(void) {
    TEST_ASSERT_EQUAL(SWO_SUCCESS, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(1, g_field_calls);
}

void test_following_chunks_are_accepted(void) {
    TEST_ASSERT_EQUAL(SWO_SUCCESS, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(SWO_SUCCESS, send(P1_FOLLOWING_CHUNK));
    TEST_ASSERT_EQUAL(2, g_tlv_calls);
}

void test_several_descriptors_are_accepted(void) {
    TEST_ASSERT_EQUAL(SWO_SUCCESS, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(SWO_SUCCESS, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(SWO_SUCCESS, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(3, g_field_calls);
}

void test_wrong_p2_rejected(void) {
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, handle_eip712_v2_field(P1_FIRST_CHUNK, 0, 0, NULL));
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, handle_eip712_v2_field(P1_FIRST_CHUNK, 1, 0, NULL));
    TEST_ASSERT_EQUAL(0, g_tlv_calls);
}

void test_unknown_p1_rejected(void) {
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, send(0x02));
    TEST_ASSERT_EQUAL(0, g_tlv_calls);
}

void test_v1_session_refused(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V1));
    TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(0, g_tlv_calls);
}

void test_no_session_refused(void) {
    eip712_session_close();
    TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(0, g_tlv_calls);
}

// only before the review: not when idle, not once the review has started, not for a transaction
void test_wrong_app_state_refused(void) {
    const uint8_t states[] = {APP_STATE_IDLE, APP_STATE_SIGNING_EIP712, APP_STATE_SIGNING_TX};

    for (size_t i = 0; i < sizeof(states); i++) {
        appState = states[i];
        TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, send(P1_FIRST_CHUNK));
    }
    TEST_ASSERT_EQUAL(0, g_tlv_calls);
}

// without a message info, or with one that announces no descriptor
void test_no_announced_descriptors_refused(void) {
    g_fields_pending = false;
    TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(0, g_tlv_calls);
}

void test_a_refused_descriptor_is_incorrect_data(void) {
    g_field_ret = false;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, send(P1_FIRST_CHUNK));
}

void test_a_malformed_payload_is_incorrect_data(void) {
    g_tlv_run_handler = false;
    g_tlv_ret = TLV_APDU_ERROR;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(0, g_field_calls);
}

void test_an_incomplete_descriptor_is_not_handled_yet(void) {
    g_tlv_run_handler = false;
    g_tlv_ret = TLV_APDU_PENDING;
    TEST_ASSERT_EQUAL(SWO_SUCCESS, send(P1_FIRST_CHUNK));
    TEST_ASSERT_EQUAL(0, g_field_calls);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocktlv_apdu_Init();
    tlv_from_apdu_StubWithCallback(tlv_from_apdu_stub);
    appState = APP_STATE_IDLE;
    eip712_session_close();
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    g_fields_pending = true;
    g_field_ret = true;
    g_field_calls = 0;
    g_tlv_ret = TLV_APDU_SUCCESS;
    g_tlv_run_handler = true;
    g_tlv_calls = 0;
}

void tearDown(void) {
    Mocktlv_apdu_Verify();
    Mocktlv_apdu_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_descriptor_is_handed_to_the_field_handler);
    RUN_TEST(test_following_chunks_are_accepted);
    RUN_TEST(test_several_descriptors_are_accepted);
    RUN_TEST(test_wrong_p2_rejected);
    RUN_TEST(test_unknown_p1_rejected);
    RUN_TEST(test_v1_session_refused);
    RUN_TEST(test_no_session_refused);
    RUN_TEST(test_wrong_app_state_refused);
    RUN_TEST(test_no_announced_descriptors_refused);
    RUN_TEST(test_a_refused_descriptor_is_incorrect_data);
    RUN_TEST(test_a_malformed_payload_is_incorrect_data);
    RUN_TEST(test_an_incomplete_descriptor_is_not_handled_yet);
    return UNITY_END();
}
