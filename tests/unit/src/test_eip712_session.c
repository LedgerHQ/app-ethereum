/**
 * @file test_eip712_session.c
 * @brief Unit tests for the typed message session at
 *        src/features/sign_message_eip712/eip712_session.c.
 *
 * A session is the only way into the preparation of a typed message, from the idle state only, and
 * closing it is the way out of its flow.
 */

#include "unity.h"
#include <stdint.h>
#include <stdbool.h>

#include "eip712_session.h"
#include "shared_context.h"

void test_open_enters_the_preparation_of_its_version(void) {
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    TEST_ASSERT_EQUAL(APP_STATE_PREPARING_EIP712, appState);
    TEST_ASSERT_TRUE(eip712_session_is_preparing(EIP712_SESSION_V2));
    TEST_ASSERT_FALSE(eip712_session_is_preparing(EIP712_SESSION_V1));
}

// another flow, or the preparation or review of another message
void test_open_refused_outside_the_idle_state(void) {
    const uint8_t states[] = {APP_STATE_SIGNING_TX,
                              APP_STATE_PREPARING_EIP712,
                              APP_STATE_SIGNING_EIP712};

    for (size_t i = 0; i < sizeof(states); ++i) {
        appState = states[i];
        TEST_ASSERT_FALSE(eip712_session_open(EIP712_SESSION_V2));
        TEST_ASSERT_EQUAL(states[i], appState);
        TEST_ASSERT_EQUAL(EIP712_SESSION_NONE, eip712_session_get());
    }
}

void test_not_preparing_once_the_review_started(void) {
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    appState = APP_STATE_SIGNING_EIP712;
    TEST_ASSERT_FALSE(eip712_session_is_preparing(EIP712_SESSION_V2));
}

void test_close_ends_the_flow_of_the_session(void) {
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    appState = APP_STATE_SIGNING_EIP712;
    eip712_session_close();
    TEST_ASSERT_EQUAL(APP_STATE_IDLE, appState);
    TEST_ASSERT_EQUAL(EIP712_SESSION_NONE, eip712_session_get());
}

// a transaction, or the review of a V0 message
void test_close_leaves_a_flow_without_a_session(void) {
    const uint8_t states[] = {APP_STATE_SIGNING_TX, APP_STATE_SIGNING_EIP712};

    for (size_t i = 0; i < sizeof(states); ++i) {
        appState = states[i];
        eip712_session_close();
        TEST_ASSERT_EQUAL(states[i], appState);
    }
}

void setUp(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_open_enters_the_preparation_of_its_version);
    RUN_TEST(test_open_refused_outside_the_idle_state);
    RUN_TEST(test_not_preparing_once_the_review_started);
    RUN_TEST(test_close_ends_the_flow_of_the_session);
    RUN_TEST(test_close_leaves_a_flow_without_a_session);
    return UNITY_END();
}
