/**
 * @file test_cmd_eip712_v2_sign.c
 * @brief Unit tests for the SIGN command of an EIP-712 V2 message at
 *        src/features/sign_message_eip712/cmd_eip712_v2_sign.c
 *
 * SIGN carries no data: it starts the hash pass and the review of what the values, and the
 * descriptors a message info announced, left in the app. Before anything is hashed or shown,
 * the descriptors received have to be the announced ones.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmd_eip712_v2_sign.h"
#include "typed_data.h"
#include "eip712_session.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "status_words.h"

extern internalStorage_t g_n_storage_writable;

// =============================================================================
// Stubs
// =============================================================================

static bool g_has_domain = true;
static bool g_has_message = true;
bool td_has_domain(void) {
    return g_has_domain;
}
bool td_has_message(void) {
    return g_has_message;
}

static bool g_hash_pass_ret = true;
static int g_hash_pass_calls = 0;
bool td_hash_pass(void) {
    g_hash_pass_calls++;
    return g_hash_pass_ret;
}

bool td_traverse_domain(td_f_value_visitor visitor, void *context) {
    (void) visitor;
    (void) context;
    return true;
}
bool td_traverse_message(td_f_value_visitor visitor, void *context) {
    (void) visitor;
    (void) context;
    return true;
}

static bool g_fields_pending = false;
static bool g_fields_match = true;
bool operation_ctx_message_has_pending_fields(void) {
    return g_fields_pending;
}
bool operation_ctx_message_fields_match(void) {
    return g_fields_match;
}

static bool g_review_ret = true;
static int g_review_calls = 0;
bool ui_712_v2_review(void) {
    g_review_calls++;
    return g_review_ret;
}

static int g_blind_signing_errors = 0;
void ui_error_blind_signing(void) {
    g_blind_signing_errors++;
}

static int g_held_error_screens = 0;
void ui_hold_error_screen(void) {
    g_held_error_screens++;
}

static uint16_t sign(void) {
    return handle_eip712_v2_sign(0, P2_EIP712_V2_IMPLEM, 0);
}

// =============================================================================
// Parameters and state
// =============================================================================

void test_a_complete_message_is_reviewed(void) {
    TEST_ASSERT_EQUAL(SWO_NO_RESPONSE, sign());
    TEST_ASSERT_EQUAL(1, g_hash_pass_calls);
    TEST_ASSERT_EQUAL(1, g_review_calls);
}

void test_wrong_p1_or_p2_rejected(void) {
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, handle_eip712_v2_sign(1, P2_EIP712_V2_IMPLEM, 0));
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, handle_eip712_v2_sign(0, 0, 0));
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, handle_eip712_v2_sign(0, 1, 0));
    TEST_ASSERT_EQUAL(0, g_review_calls);
}

void test_data_rejected(void) {
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, handle_eip712_v2_sign(0, P2_EIP712_V2_IMPLEM, 1));
    TEST_ASSERT_EQUAL(0, g_review_calls);
}

void test_v1_session_refused(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V1));
    TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, sign());
    TEST_ASSERT_EQUAL(0, g_hash_pass_calls);
}

void test_no_session_refused(void) {
    eip712_session_close();
    TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, sign());
    TEST_ASSERT_EQUAL(0, g_hash_pass_calls);
}

void test_wrong_app_state_refused(void) {
    const uint8_t states[] = {APP_STATE_IDLE, APP_STATE_SIGNING_EIP712, APP_STATE_SIGNING_TX};

    for (size_t i = 0; i < sizeof(states); i++) {
        appState = states[i];
        TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, sign());
    }
    TEST_ASSERT_EQUAL(0, g_hash_pass_calls);
}

void test_missing_domain_or_message_refused(void) {
    g_has_domain = false;
    TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, sign());
    g_has_domain = true;
    g_has_message = false;
    TEST_ASSERT_EQUAL(SWO_COMMAND_NOT_ALLOWED, sign());
    TEST_ASSERT_EQUAL(0, g_hash_pass_calls);
}

// =============================================================================
// The announced descriptors
// =============================================================================

void test_the_announced_descriptors_let_it_go_on(void) {
    g_fields_pending = true;
    g_fields_match = true;
    TEST_ASSERT_EQUAL(SWO_NO_RESPONSE, sign());
    TEST_ASSERT_EQUAL(1, g_review_calls);
}

// nothing is hashed, nothing is shown
void test_other_descriptors_than_the_announced_are_refused(void) {
    g_fields_pending = true;
    g_fields_match = false;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, sign());
    TEST_ASSERT_EQUAL(0, g_hash_pass_calls);
    TEST_ASSERT_EQUAL(0, g_review_calls);
    TEST_ASSERT_EQUAL(0, g_blind_signing_errors);
}

// without announcement, whatever the match says is not asked
void test_no_announcement_means_nothing_to_match(void) {
    g_fields_pending = false;
    g_fields_match = false;
    TEST_ASSERT_EQUAL(SWO_NO_RESPONSE, sign());
}

// =============================================================================
// Hash pass and review
// =============================================================================

void test_hash_pass_failure_is_incorrect_data(void) {
    g_hash_pass_ret = false;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, sign());
    TEST_ASSERT_EQUAL(0, g_review_calls);
}

void test_raw_review_needs_blind_signing_to_be_enabled(void) {
    g_n_storage_writable.dataAllowed = false;
    g_n_storage_writable.verbose_eip712 = false;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, sign());
    TEST_ASSERT_EQUAL(1, g_blind_signing_errors);
    // the error screen stays up, rather than the home screen the error reply brings back
    TEST_ASSERT_EQUAL(1, g_held_error_screens);
    TEST_ASSERT_EQUAL(0, g_review_calls);
}

void test_review_failure_is_incorrect_data(void) {
    g_review_ret = false;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, sign());
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    g_has_domain = true;
    g_has_message = true;
    g_hash_pass_ret = true;
    g_hash_pass_calls = 0;
    g_fields_pending = false;
    g_fields_match = true;
    g_review_ret = true;
    g_review_calls = 0;
    g_blind_signing_errors = 0;
    g_n_storage_writable.dataAllowed = true;
    g_n_storage_writable.verbose_eip712 = false;
}

void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_complete_message_is_reviewed);
    RUN_TEST(test_wrong_p1_or_p2_rejected);
    RUN_TEST(test_data_rejected);
    RUN_TEST(test_v1_session_refused);
    RUN_TEST(test_no_session_refused);
    RUN_TEST(test_wrong_app_state_refused);
    RUN_TEST(test_missing_domain_or_message_refused);
    RUN_TEST(test_the_announced_descriptors_let_it_go_on);
    RUN_TEST(test_other_descriptors_than_the_announced_are_refused);
    RUN_TEST(test_no_announcement_means_nothing_to_match);
    RUN_TEST(test_hash_pass_failure_is_incorrect_data);
    RUN_TEST(test_raw_review_needs_blind_signing_to_be_enabled);
    RUN_TEST(test_review_failure_is_incorrect_data);
    return UNITY_END();
}
