/**
 * @file test_call_description.c
 * @brief Unit tests for the rules of the description of a call at
 *        src/features/generic_tx_parser/call_description.c.
 *
 * Which flows may be told about a call, and which calls are nested in another operation, both
 * follow from the state of the app, the typed message session and what the operations are.
 * operation_ctx is stubbed to put the operations in the state each test needs.
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
static bool g_has_message = false;
static bool g_is_describing_call = false;
static size_t g_call_count = 0;

bool operation_ctx_is_root(void) {
    return g_is_root;
}
bool operation_ctx_has_message(void) {
    return g_has_message;
}
bool operation_ctx_is_describing_call(void) {
    return g_is_describing_call;
}
size_t get_operation_ctx_count(void) {
    return g_call_count;
}

static void reset(void) {
    appState = APP_STATE_IDLE;
    eip712_session_close();
    g_is_root = false;
    g_has_message = false;
    g_is_describing_call = false;
    g_call_count = 0;
}

// the state a typed message of the given version is in once its review started
static void start_review(e_eip712_session session) {
    appState = APP_STATE_IDLE;
    TEST_ASSERT_TRUE(eip712_session_open(session));
    appState = APP_STATE_SIGNING_EIP712;
}

// =============================================================================
// call_description_is_allowed
// =============================================================================

// a call is described while signing its transaction
void test_a_call_is_described_while_signing_a_transaction(void) {
    appState = APP_STATE_SIGNING_TX;
    TEST_ASSERT_TRUE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
    TEST_ASSERT_TRUE(call_description_is_allowed(CALL_DESCRIPTION_FIELD));
}

// a V1 message enters the signing state at its first filter, and describes its calls from there. A
// V2 one is in that state once its review started, which leaves nothing to describe
void test_a_call_is_described_in_the_signing_state_of_a_v1_message_only(void) {
    start_review(EIP712_SESSION_V1);
    TEST_ASSERT_TRUE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
    TEST_ASSERT_TRUE(call_description_is_allowed(CALL_DESCRIPTION_FIELD));
    eip712_session_close();
    start_review(EIP712_SESSION_V2);
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_FIELD));
    eip712_session_close();
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
}

// a V2 message has the info of a call described once its descriptors named one, and the
// descriptors of that call once its info matched it
void test_a_call_of_a_typed_message_is_described_while_it_waits_for_its_review(void) {
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    g_has_message = true;
    // no call named yet
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_FIELD));
    // a call named, and not matched yet
    g_call_count = 1;
    TEST_ASSERT_TRUE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_FIELD));
    // matched
    g_is_describing_call = true;
    TEST_ASSERT_TRUE(call_description_is_allowed(CALL_DESCRIPTION_FIELD));
}

void test_a_call_is_not_described_in_the_preparation_of_something_else_than_a_message(void) {
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V2));
    g_call_count = 1;
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
}

void test_a_call_is_not_described_in_a_v1_session_of_a_typed_message(void) {
    TEST_ASSERT_TRUE(eip712_session_open(EIP712_SESSION_V1));
    g_has_message = true;
    g_call_count = 1;
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
}

void test_a_call_is_not_described_in_the_preparation_of_a_message_without_a_session(void) {
    appState = APP_STATE_PREPARING_EIP712;
    g_has_message = true;
    g_call_count = 1;
    TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
}

void test_a_call_is_not_described_in_another_state(void) {
    const uint8_t states[] = {APP_STATE_IDLE, APP_STATE_SIGNING_MESSAGE};

    for (size_t i = 0; i < sizeof(states); i++) {
        appState = states[i];
        TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_INFO));
        TEST_ASSERT_FALSE(call_description_is_allowed(CALL_DESCRIPTION_FIELD));
    }
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
    RUN_TEST(test_a_call_is_described_while_signing_a_transaction);
    RUN_TEST(test_a_call_is_described_in_the_signing_state_of_a_v1_message_only);
    RUN_TEST(test_a_call_of_a_typed_message_is_described_while_it_waits_for_its_review);
    RUN_TEST(test_a_call_is_not_described_in_the_preparation_of_something_else_than_a_message);
    RUN_TEST(test_a_call_is_not_described_in_a_v1_session_of_a_typed_message);
    RUN_TEST(test_a_call_is_not_described_in_the_preparation_of_a_message_without_a_session);
    RUN_TEST(test_a_call_is_not_described_in_another_state);
    RUN_TEST(test_calls_are_nested_but_the_root_of_a_transaction);
    RUN_TEST(test_the_root_call_of_a_v1_message_is_nested);
    RUN_TEST(test_the_root_call_is_not_nested_in_the_review_of_another_message);
    return UNITY_END();
}
