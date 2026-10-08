/**
 * @file test_call_description.c
 * @brief Unit tests for the rules of the description of a call at
 *        src/features/generic_tx_parser/call_description.c.
 *
 * Which calls are nested in another operation follows from the state of the app, the typed message
 * session and what the operations are. operation_ctx is stubbed to put the operations in the state
 * each test needs.
 */

#include "unity.h"
#include <stdint.h>
#include <stdbool.h>

#include "call_description.h"
#include "operation_ctx.h"
#include "eip712_session.h"
#include "shared_context.h"

// =============================================================================
// Stubs for the operations
// =============================================================================

static bool g_is_root = false;

bool operation_ctx_is_root(void) {
    return g_is_root;
}

static void reset(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
    g_is_root = false;
}

// the state a typed message of the given version is in once its review started
static void start_review(e_eip712_session session) {
    appState = APP_STATE_IDLE;
    TEST_ASSERT_TRUE(eip712_session_open(session));
    appState = APP_STATE_SIGNING_EIP712;
}

// =============================================================================
// call_description_is_nested
// =============================================================================

// every call but the root of a transaction is nested in another operation
void test_calls_are_nested_but_the_root_of_a_transaction(void) {
    appState = APP_STATE_SIGNING_TX;
    g_is_root = true;
    TEST_ASSERT_FALSE(call_description_is_nested());
    g_is_root = false;
    TEST_ASSERT_TRUE(call_description_is_nested());
}

// the first call of a V1 message is its root, and nested in it all the same
void test_the_root_call_of_a_v1_message_is_nested(void) {
    g_is_root = true;
    start_review(EIP712_SESSION_V1);
    TEST_ASSERT_TRUE(call_description_is_nested());
}

// the review of a V2 message is in the same state, but has no call left to describe
void test_the_root_call_is_not_nested_in_the_review_of_another_message(void) {
    g_is_root = true;
    appState = APP_STATE_SIGNING_EIP712;
    TEST_ASSERT_FALSE(call_description_is_nested());
    start_review(EIP712_SESSION_V2);
    TEST_ASSERT_FALSE(call_description_is_nested());
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
    RUN_TEST(test_calls_are_nested_but_the_root_of_a_transaction);
    RUN_TEST(test_the_root_call_of_a_v1_message_is_nested);
    RUN_TEST(test_the_root_call_is_not_nested_in_the_review_of_another_message);
    return UNITY_END();
}
