/**
 * @file test_cmd_field_tx_info.c
 * @brief Unit tests for the two transaction APDU command entry points at
 *        src/features/generic_tx_parser/cmd_tx_field.c (with the shared
 *        src/features/gcs/cmd_field.c) and
 *        src/features/generic_tx_parser/cmd_tx_info.c.
 *
 * Both commands are thin wrappers around the deeper parsing layers:
 * each one gates on appState (must be SIGNING_TX or SIGNING_EIP712),
 * handle_tx_field additionally requires that a current tx_info has been
 * registered, then both delegate to tlv_from_apdu() which streams the
 * TLV payload into the static handle_tlv_payload.
 *
 * The deep parsing path is already covered by test_operation_info /
 * test_field_validation / test_operation_ctx; this slice focuses on the
 * entry-point glue: status-word returns on guard failures and on
 * tlv_from_apdu success / failure.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmd_field.h"
#include "cmd_tx_field.h"
#include "cmd_tx_info.h"
#include "operation_info.h"
#include "gcs_field.h"
#include "operation_ctx.h"
#include "call_description.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "status_words.h"
#include "Mocktlv_apdu.h"
#include "cx.h"
// wraps.h removed: all stubs are local

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// tlv_from_apdu is the seam — control its return value per test.
// Caller treats return as a bool (`if (!tlv_from_apdu(...))`), so
// TLV_APDU_ERROR == 0 means "fail" and any non-zero (PENDING, SUCCESS)
// is "ok".
static e_tlv_apdu_ret g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
// When true, the wrap actually invokes the handler with an empty buffer.
// Used by tests that want to exercise the static handle_tlv_payload helpers.
static bool g_field_tlv_invoke_handler = false;
static bool g_field_tlv_handler_returned = false;
static e_tlv_apdu_ret tlv_from_apdu_stub(bool first_chunk,
                                         uint8_t lc,
                                         const uint8_t *payload,
                                         f_tlv_payload_handler handler,
                                         int cmock_num_calls) {
    (void) first_chunk;
    (void) lc;
    (void) payload;
    (void) cmock_num_calls;
    if (g_field_tlv_invoke_handler && handler != NULL) {
        buffer_t buf = {.ptr = NULL, .size = 0, .offset = 0};
        g_field_tlv_handler_returned = handler(&buf);
    }
    return g_field_tlv_from_apdu_ret;
}

// handle_tx_field reads from get_current_operation_info — control it via a local stub.
static const s_operation_info *s_tx_info_ret = NULL;
const s_operation_info *get_current_operation_info(void) {
    return s_tx_info_ret;
}

// handle_tx_field calls gcs_cleanup on the no-tx-info path — count calls.
static int g_gcs_cleanup_calls = 0;
void gcs_cleanup(void) {
    g_gcs_cleanup_calls++;
}

// The static handle_tlv_payload helpers reference these symbols. Each
// has a controllable return value so the test cases below can drive
// the static helpers' branches via the wrap-invokes-handler path.
static bool g_handle_field_struct_ret = true;
static bool g_verify_field_struct_ret = true;
static bool g_format_field_ret = true;
static cx_err_t g_cx_hash_ret = CX_OK;
// validate_instruction_hash is consumed inside a `while (...)` loop in
// cmd_tx_field's static handle_tlv_payload, so an unconditional `true`
// stub would loop forever. Drive it with a budget instead: each call
// decrements the counter; when it reaches zero the stub returns false
// and the loop exits.
static int g_validate_instruction_hash_budget = 0;
static bool g_operation_ctx_is_root_ret = true;
static bool g_process_empty_calls_after_ret = true;
static bool g_process_empty_calls_before_ret = true;
static bool g_receive_tx_info_ret = true;
static uint8_t g_received_selector[CALLDATA_SELECTOR_SIZE];
static int g_delete_tx_info_calls = 0;
// the transaction contexts known, and the selector of the calldata parked for the first one
static size_t g_operation_ctx_count = 1;
static const uint8_t *g_parked_selector = NULL;
static bool g_find_matching_operation_ctx_ret = true;
static bool g_operation_ctx_set_info_ret = true;
static int g_cleanup_field_calls = 0;
static int g_operation_ctx_pop_calls = 0;
static bool g_operation_ctx_pop_ret = true;

bool handle_field_struct(const buffer_t *buf, s_field_ctx *ctx) {
    (void) buf;
    (void) ctx;
    return g_handle_field_struct_ret;
}
void cleanup_field(s_field *field) {
    (void) field;
    g_cleanup_field_calls++;
}
static bool g_fields_hash_ctx_none = false;
cx_hash_t *get_current_operation_fields_hash_ctx(void) {
    static cx_sha3_t dummy;
    return g_fields_hash_ctx_none ? NULL : (cx_hash_t *) &dummy;
}
cx_err_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t l,
                          uint8_t *out,
                          size_t ol) {
    (void) hash;
    (void) mode;
    (void) in;
    (void) l;
    (void) out;
    (void) ol;
    return g_cx_hash_ret;
}
bool verify_field_struct(const s_field_ctx *ctx) {
    (void) ctx;
    return g_verify_field_struct_ret;
}
bool format_field(s_field *f, uint8_t depth) {
    (void) f;
    (void) depth;
    return g_format_field_ret;
}
bool process_empty_calls_after(void) {
    return g_process_empty_calls_after_ret;
}
bool process_empty_calls_before(void) {
    return g_process_empty_calls_before_ret;
}
bool operation_ctx_pop(void) {
    g_operation_ctx_pop_calls++;
    return g_operation_ctx_pop_ret;
}
// whether the flow is in a state to be told about a call, and what the command brings to its
// description
static bool g_can_describe_call_ret = true;
static e_call_description g_described_part = CALL_DESCRIPTION_INFO;
bool call_description_is_allowed(e_call_description part) {
    g_described_part = part;
    return g_can_describe_call_ret;
}
// every call but the root of a transaction is nested in another operation
bool call_description_is_nested(void) {
    return !g_operation_ctx_is_root_ret || (appState == APP_STATE_SIGNING_EIP712);
}
// what starts the review of a nested call: its intent, and whether it announces descriptors
static bool g_set_intent_field_ret = true;
static int g_set_intent_field_calls = 0;
bool set_intent_field(const char *intent) {
    (void) intent;
    g_set_intent_field_calls++;
    return g_set_intent_field_ret;
}
static bool g_fields_hash_is_empty_ret = true;
static bool g_fields_hash_is_empty_ok = true;
bool operation_info_fields_hash_is_empty(const s_operation_info *info, bool *is_empty) {
    (void) info;
    *is_empty = g_fields_hash_is_empty_ret;
    return g_fields_hash_is_empty_ok;
}
bool validate_instruction_hash(void) {
    if (g_validate_instruction_hash_budget > 0) {
        g_validate_instruction_hash_budget--;
        return true;
    }
    return false;
}
s_operation_info *new_operation_info(const buffer_t *buf, e_operation_info_context context) {
    static s_operation_info received;

    (void) buf;
    received.context = context;
    memcpy(received.selector, g_received_selector, sizeof(received.selector));
    return g_receive_tx_info_ret ? &received : NULL;
}
size_t get_operation_ctx_count(void) {
    return g_operation_ctx_count;
}
const uint8_t *calldata_get_selector(const s_calldata *calldata) {
    (void) calldata;
    return g_parked_selector;
}
const uint8_t *get_selector(const s_operation_info *node) {
    return node->selector;
}
void delete_operation_info(s_operation_info *node) {
    (void) node;
    g_delete_tx_info_calls++;
}
bool find_matching_operation_ctx(const uint8_t *a, const uint8_t *s, const uint64_t *c) {
    (void) a;
    (void) s;
    (void) c;
    return g_find_matching_operation_ctx_ret;
}
bool operation_ctx_set_info(s_operation_info *info) {
    (void) info;
    return g_operation_ctx_set_info_ret;
}
// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    appState = APP_STATE_IDLE;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    g_field_tlv_invoke_handler = false;
    g_field_tlv_handler_returned = false;
    s_tx_info_ret = NULL;
    g_gcs_cleanup_calls = 0;
    g_handle_field_struct_ret = true;
    g_verify_field_struct_ret = true;
    g_format_field_ret = true;
    g_cx_hash_ret = CX_OK;
    g_fields_hash_ctx_none = false;
    g_validate_instruction_hash_budget = 0;
    g_can_describe_call_ret = true;
    g_operation_ctx_is_root_ret = true;
    g_process_empty_calls_after_ret = true;
    g_process_empty_calls_before_ret = true;
    g_receive_tx_info_ret = true;
    memset(g_received_selector, 0, sizeof(g_received_selector));
    g_delete_tx_info_calls = 0;
    g_operation_ctx_count = 1;
    g_parked_selector = NULL;
    g_find_matching_operation_ctx_ret = true;
    g_operation_ctx_set_info_ret = true;
    g_set_intent_field_ret = true;
    g_set_intent_field_calls = 0;
    g_fields_hash_is_empty_ret = true;
    g_fields_hash_is_empty_ok = true;
    g_cleanup_field_calls = 0;
    g_operation_ctx_pop_calls = 0;
    g_operation_ctx_pop_ret = true;
}

// =============================================================================
// handle_tx_field — appState guard
// =============================================================================

// A transaction is described with P2 00, a typed message with P2 02 to another command
void test_field_p2_rejected(void) {
    appState = APP_STATE_SIGNING_TX;
    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, 1, 0, NULL), SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, P2_EIP712_V2_IMPLEM, 0, NULL),
                      SWO_INCORRECT_P1_P2);
}

void test_field_refused_when_the_flow_cannot_describe_a_call(void) {
    g_can_describe_call_ret = false;
    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
}

// it brings the descriptors of a call, which a typed message only takes for the call it matched
void test_field_asks_about_the_descriptors_of_a_call(void) {
    static const s_operation_info dummy = {0};

    s_tx_info_ret = &dummy;
    g_described_part = CALL_DESCRIPTION_INFO;
    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(CALL_DESCRIPTION_FIELD, g_described_part);
}

// =============================================================================
// handle_tx_field — no current tx_info
// =============================================================================

void test_field_no_tx_info_triggers_cleanup(void) {
    appState = APP_STATE_SIGNING_TX;
    s_tx_info_ret = NULL;
    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
    // gcs_cleanup must NOT be called here: if a GCS review is on-screen,
    // calling it would free buffers that NBGL still holds, causing a crash.
    TEST_ASSERT_EQUAL(g_gcs_cleanup_calls, 0);
}

// =============================================================================
// handle_tx_field — happy path delegates to tlv_from_apdu
// =============================================================================

void test_field_tlv_failure_returns_incorrect_data(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_ERROR;
    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_INCORRECT_DATA);
    // gcs_cleanup is NOT called on this path — only on the no-tx-info
    // branch.
    TEST_ASSERT_EQUAL(g_gcs_cleanup_calls, 0);
}

void test_field_tlv_success_returns_success(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

// =============================================================================
// handle_tx_info — appState guard
// =============================================================================

void test_tx_info_refused_when_the_flow_cannot_describe_a_call(void) {
    g_can_describe_call_ret = false;
    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
}

// it brings the info of a call, which a typed message only takes once its descriptors named some
void test_tx_info_asks_about_the_info_of_a_call(void) {
    g_described_part = CALL_DESCRIPTION_FIELD;
    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(CALL_DESCRIPTION_INFO, g_described_part);
}

void test_tx_info_p2_rejected(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 1, 0, NULL), SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, P2_EIP712_V2_IMPLEM, 0, NULL),
                      SWO_INCORRECT_P1_P2);
}

// =============================================================================
// handle_tx_info — tlv_from_apdu paths
// =============================================================================

void test_tx_info_tlv_failure_returns_incorrect_data(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_ERROR;
    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_INCORRECT_DATA);
}

void test_tx_info_tlv_success_returns_success(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

// =============================================================================
// Internal handle_tlv_payload — invoked through the wrap so we can hit
// the static helpers' branches in cmd_tx_field.c / cmd_tx_info.c.
// =============================================================================

// --- cmd_tx_field internal handler ---

void test_field_handler_happy_path(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_is_root_ret = true;  // not signing EIP-712 & at root → exit while loop
    g_validate_instruction_hash_budget = 0;

    TEST_ASSERT_EQUAL(handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_TRUE(g_field_tlv_handler_returned);
    // cleanup_field is NOT called on happy path
    TEST_ASSERT_EQUAL(g_cleanup_field_calls, 0);
}

void test_field_handler_handle_field_struct_failure(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_handle_field_struct_ret = false;

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    // cleanup_field must be called to release any half-built state
    TEST_ASSERT_EQUAL(g_cleanup_field_calls, 1);
}

// The descriptors have to be attested by something to be accepted
void test_field_handler_without_a_fields_hash_refused(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_fields_hash_ctx_none = true;

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_cleanup_field_calls, 1);
}

void test_field_handler_hash_failure(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_cx_hash_ret = 0x1234;  // not CX_OK

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_cleanup_field_calls, 1);
}

void test_field_handler_verify_failure(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_verify_field_struct_ret = false;

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_cleanup_field_calls, 1);
}

void test_field_handler_format_failure(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_format_field_ret = false;

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    // format_field failure does NOT trigger cleanup_field (different path)
    TEST_ASSERT_EQUAL(g_cleanup_field_calls, 0);
}

void test_field_handler_eip712_pop_loop_runs(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_EIP712;  // forces the while loop entry
    g_field_tlv_invoke_handler = true;
    // The validator returns true twice then false → two pops, then exit.
    g_validate_instruction_hash_budget = 2;

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 2);
}

// a complete call that cannot be dropped leaves a description that cannot go on
void test_field_handler_pop_failure_propagates(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_EIP712;  // forces the while loop entry
    g_field_tlv_invoke_handler = true;
    g_validate_instruction_hash_budget = 2;
    g_operation_ctx_pop_ret = false;

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 1);
}

// =============================================================================
// The calls a typed message names
// =============================================================================

void test_field_handler_process_empty_failure_propagates(void) {
    static const s_operation_info dummy = {0};
    s_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_EIP712;
    g_field_tlv_invoke_handler = true;
    g_validate_instruction_hash_budget = 1;   // first iter true, then false
    g_process_empty_calls_after_ret = false;  // bail out of while loop

    handle_tx_field(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
}

// --- cmd_tx_info internal handler ---

void test_tx_info_handler_happy_path(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;

    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_TRUE(g_field_tlv_handler_returned);
}

void test_tx_info_handler_receive_failure(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_receive_tx_info_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    // nothing was received, so there is nothing to free
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 0);
}

// The first transaction has no context yet: what describes it is checked against the calldata
// parked for it, which is the only thing there is to tell it to belong to this transaction
void test_tx_info_handler_selector_matches_parked_calldata(void) {
    static const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_count = 0;
    g_parked_selector = selector;
    memcpy(g_received_selector, selector, sizeof(selector));

    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_TRUE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 0);
}

void test_tx_info_handler_selector_mismatch_rejected(void) {
    static const uint8_t parked[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    static const uint8_t other[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};

    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_count = 0;
    g_parked_selector = parked;
    memcpy(g_received_selector, other, sizeof(other));

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 1);
}

void test_tx_info_handler_no_parked_calldata_rejected(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_count = 0;
    g_parked_selector = NULL;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 1);
}

// Once a transaction context exists, find_matching_operation_ctx() does the matching
void test_tx_info_handler_selector_not_checked_once_a_context_exists(void) {
    static const uint8_t parked[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_count = 1;
    g_parked_selector = parked;

    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_TRUE(g_field_tlv_handler_returned);
}

void test_tx_info_handler_no_matching_ctx_rejected(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_find_matching_operation_ctx_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    // the received tx info is not attached to any context, so it must be released
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 1);
}

void test_tx_info_handler_set_info_failure_propagates(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_set_info_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    // no call took the info, so it must be released
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 1);
}

// the root of a transaction is not told apart by its operation, and is ended by its signing
void test_tx_info_handler_of_the_root_of_a_transaction_is_not_started(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_is_root_ret = true;

    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_set_intent_field_calls, 0);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 0);
}

// a nested call is told apart by its operation, and is still described if it announces descriptors
void test_tx_info_handler_of_a_nested_call_shows_its_intent(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_is_root_ret = false;
    g_fields_hash_is_empty_ret = false;

    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_set_intent_field_calls, 1);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 0);
}

// a nested call that announces none has nothing more to wait for
void test_tx_info_handler_of_a_nested_call_without_descriptors_is_done(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_is_root_ret = false;
    g_fields_hash_is_empty_ret = true;

    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_set_intent_field_calls, 1);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 1);
}

// the first call of a V1 message is its root, and nested in it all the same
void test_tx_info_handler_of_the_root_of_a_v1_message_is_started(void) {
    appState = APP_STATE_SIGNING_EIP712;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_is_root_ret = true;

    TEST_ASSERT_EQUAL(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_set_intent_field_calls, 1);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 1);
}

// the info is the call's by then, which releases it with the call
void test_tx_info_handler_of_a_nested_call_failures_propagate(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_operation_ctx_is_root_ret = false;

    g_set_intent_field_ret = false;
    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 0);

    g_set_intent_field_ret = true;
    g_fields_hash_is_empty_ok = false;
    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 0);

    g_fields_hash_is_empty_ok = true;
    g_operation_ctx_pop_ret = false;
    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    TEST_ASSERT_EQUAL(g_operation_ctx_pop_calls, 1);
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 0);
}

void test_tx_info_handler_process_empty_failure_frees_tx_info(void) {
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_process_empty_calls_before_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    TEST_ASSERT_FALSE(g_field_tlv_handler_returned);
    // The not-yet-attached info must not outlive the failed APDU.
    TEST_ASSERT_EQUAL(g_delete_tx_info_calls, 1);
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
    RUN_TEST(test_field_p2_rejected);
    RUN_TEST(test_field_refused_when_the_flow_cannot_describe_a_call);
    RUN_TEST(test_field_asks_about_the_descriptors_of_a_call);
    RUN_TEST(test_field_no_tx_info_triggers_cleanup);
    RUN_TEST(test_field_tlv_failure_returns_incorrect_data);
    RUN_TEST(test_field_tlv_success_returns_success);
    RUN_TEST(test_tx_info_refused_when_the_flow_cannot_describe_a_call);
    RUN_TEST(test_tx_info_asks_about_the_info_of_a_call);
    RUN_TEST(test_tx_info_p2_rejected);
    RUN_TEST(test_tx_info_tlv_failure_returns_incorrect_data);
    RUN_TEST(test_tx_info_tlv_success_returns_success);
    RUN_TEST(test_field_handler_happy_path);
    RUN_TEST(test_field_handler_handle_field_struct_failure);
    RUN_TEST(test_field_handler_without_a_fields_hash_refused);
    RUN_TEST(test_field_handler_hash_failure);
    RUN_TEST(test_field_handler_verify_failure);
    RUN_TEST(test_field_handler_format_failure);
    RUN_TEST(test_field_handler_eip712_pop_loop_runs);
    RUN_TEST(test_field_handler_pop_failure_propagates);
    RUN_TEST(test_field_handler_process_empty_failure_propagates);
    RUN_TEST(test_tx_info_handler_happy_path);
    RUN_TEST(test_tx_info_handler_receive_failure);
    RUN_TEST(test_tx_info_handler_selector_matches_parked_calldata);
    RUN_TEST(test_tx_info_handler_selector_mismatch_rejected);
    RUN_TEST(test_tx_info_handler_no_parked_calldata_rejected);
    RUN_TEST(test_tx_info_handler_selector_not_checked_once_a_context_exists);
    RUN_TEST(test_tx_info_handler_no_matching_ctx_rejected);
    RUN_TEST(test_tx_info_handler_set_info_failure_propagates);
    RUN_TEST(test_tx_info_handler_of_the_root_of_a_transaction_is_not_started);
    RUN_TEST(test_tx_info_handler_of_a_nested_call_shows_its_intent);
    RUN_TEST(test_tx_info_handler_of_a_nested_call_without_descriptors_is_done);
    RUN_TEST(test_tx_info_handler_of_the_root_of_a_v1_message_is_started);
    RUN_TEST(test_tx_info_handler_of_a_nested_call_failures_propagate);
    RUN_TEST(test_tx_info_handler_process_empty_failure_frees_tx_info);
    return UNITY_END();
}
