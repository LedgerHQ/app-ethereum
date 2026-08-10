/**
 * @file test_cmd_field_tx_info.c
 * @brief Unit tests for the two GCS APDU command entry points at
 *        src/features/generic_tx_parser/cmd_field.c and
 *        src/features/generic_tx_parser/cmd_tx_info.c.
 *
 * Both commands are thin wrappers around the deeper parsing layers:
 * each one gates on appState (must be SIGNING_TX or SIGNING_EIP712),
 * cmd_field additionally requires that a current tx_info has been
 * registered, then both delegate to tlv_from_apdu() which streams the
 * TLV payload into the static handle_tlv_payload.
 *
 * The deep parsing path is already covered by test_tx_info /
 * test_field_validation / test_tx_ctx; this slice focuses on the
 * entry-point glue: status-word returns on guard failures and on
 * tlv_from_apdu success / failure.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmd_field.h"
#include "cmd_tx_info.h"
#include "gtp_tx_info.h"
#include "gtp_field.h"
#include "tx_ctx.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "status_words.h"
#include "tlv_apdu.h"
#include "cx.h"
#include "wraps.h"

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
e_tlv_apdu_ret __wrap_tlv_from_apdu(bool first_chunk,
                                    uint8_t lc,
                                    const uint8_t *payload,
                                    f_tlv_payload_handler handler) {
    (void) first_chunk;
    (void) lc;
    (void) payload;
    if (g_field_tlv_invoke_handler && handler != NULL) {
        buffer_t buf = {.ptr = NULL, .size = 0, .offset = 0};
        g_field_tlv_handler_returned = handler(&buf);
    }
    return g_field_tlv_from_apdu_ret;
}

// cmd_field reads from get_current_tx_info — control it via a wrap.

// cmd_field calls gcs_cleanup on the no-tx-info path — count calls.
static int g_gcs_cleanup_calls = 0;
void __wrap_gcs_cleanup(void) {
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
// cmd_field's static handle_tlv_payload, so an unconditional `true`
// stub would loop forever. Drive it with a budget instead: each call
// decrements the counter; when it reaches zero the stub returns false
// and the loop exits.
static int g_validate_instruction_hash_budget = 0;
static bool g_tx_ctx_is_root_ret = true;
static bool g_process_empty_txs_after_ret = true;
static bool g_process_empty_txs_before_ret = true;
static bool g_handle_tx_info_struct_ret = true;
static bool g_verify_tx_info_struct_ret = true;
static bool g_find_matching_tx_ctx_ret = true;
static bool g_set_tx_info_into_tx_ctx_ret = true;
static int g_cleanup_field_calls = 0;
static int g_tx_ctx_pop_calls = 0;

bool handle_field_struct(const buffer_t *buf, s_field_ctx *ctx) {
    (void) buf;
    (void) ctx;
    return g_handle_field_struct_ret;
}
void cleanup_field(s_field *field) {
    (void) field;
    g_cleanup_field_calls++;
}
cx_hash_t *get_fields_hash_ctx(void) {
    static cx_sha3_t dummy;
    return (cx_hash_t *) &dummy;
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
bool process_empty_txs_after(void) {
    return g_process_empty_txs_after_ret;
}
bool process_empty_txs_before(void) {
    return g_process_empty_txs_before_ret;
}
void tx_ctx_pop(void) {
    g_tx_ctx_pop_calls++;
}
bool tx_ctx_is_root(void) {
    return g_tx_ctx_is_root_ret;
}
bool validate_instruction_hash(void) {
    if (g_validate_instruction_hash_budget > 0) {
        g_validate_instruction_hash_budget--;
        return true;
    }
    return false;
}
bool handle_tx_info_struct(const buffer_t *buf, s_tx_info_ctx *ctx) {
    (void) buf;
    (void) ctx;
    return g_handle_tx_info_struct_ret;
}
bool verify_tx_info_struct(const s_tx_info_ctx *ctx) {
    (void) ctx;
    return g_verify_tx_info_struct_ret;
}
bool find_matching_tx_ctx(const uint8_t *a, const uint8_t *s, const uint64_t *c) {
    (void) a;
    (void) s;
    (void) c;
    return g_find_matching_tx_ctx_ret;
}
bool set_tx_info_into_tx_ctx(s_tx_info *info) {
    (void) info;
    return g_set_tx_info_into_tx_ctx_ret;
}
// =============================================================================
// Fixture
// =============================================================================

static int reset(void **state) {
    (void) state;
    appState = APP_STATE_IDLE;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    g_field_tlv_invoke_handler = false;
    g_field_tlv_handler_returned = false;
    g_tx_info_ret = NULL;
    g_gcs_cleanup_calls = 0;
    g_handle_field_struct_ret = true;
    g_verify_field_struct_ret = true;
    g_format_field_ret = true;
    g_cx_hash_ret = CX_OK;
    g_validate_instruction_hash_budget = 0;
    g_tx_ctx_is_root_ret = true;
    g_process_empty_txs_after_ret = true;
    g_process_empty_txs_before_ret = true;
    g_handle_tx_info_struct_ret = true;
    g_verify_tx_info_struct_ret = true;
    g_find_matching_tx_ctx_ret = true;
    g_set_tx_info_into_tx_ctx_ret = true;
    g_cleanup_field_calls = 0;
    g_tx_ctx_pop_calls = 0;
    return 0;
}

// =============================================================================
// handle_field — appState guard
// =============================================================================

static void test_field_wrong_appstate_rejected(void **state) {
    (void) state;
    appState = APP_STATE_IDLE;  // not SIGNING_TX / SIGNING_EIP712
    assert_int_equal(handle_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
}

static void test_field_signing_message_state_rejected(void **state) {
    (void) state;
    // Other signing states (MESSAGE, EIP7702) also do not allow field cmds.
    appState = APP_STATE_SIGNING_MESSAGE;
    assert_int_equal(handle_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
}

// =============================================================================
// handle_field — no current tx_info
// =============================================================================

static void test_field_no_tx_info_triggers_cleanup(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_tx_info_ret = NULL;
    assert_int_equal(handle_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
    // gcs_cleanup must NOT be called here: if a GCS review is on-screen,
    // calling it would free buffers that NBGL still holds, causing a crash.
    assert_int_equal(g_gcs_cleanup_calls, 0);
}

// =============================================================================
// handle_field — happy path delegates to tlv_from_apdu
// =============================================================================

static void test_field_tlv_failure_returns_incorrect_data(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_ERROR;
    assert_int_equal(handle_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_INCORRECT_DATA);
    // gcs_cleanup is NOT called on this path — only on the no-tx-info
    // branch.
    assert_int_equal(g_gcs_cleanup_calls, 0);
}

static void test_field_tlv_success_returns_success(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    assert_int_equal(handle_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

static void test_field_signing_eip712_allowed(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_EIP712;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    assert_int_equal(handle_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

// =============================================================================
// handle_tx_info — appState guard
// =============================================================================

static void test_tx_info_wrong_appstate_rejected(void **state) {
    (void) state;
    appState = APP_STATE_IDLE;
    assert_int_equal(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
}

static void test_tx_info_signing_message_state_rejected(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_MESSAGE;
    assert_int_equal(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_COMMAND_NOT_ALLOWED);
}

// =============================================================================
// handle_tx_info — tlv_from_apdu paths
// =============================================================================

static void test_tx_info_tlv_failure_returns_incorrect_data(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_ERROR;
    assert_int_equal(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_INCORRECT_DATA);
}

static void test_tx_info_tlv_success_returns_success(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    assert_int_equal(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

static void test_tx_info_eip712_signing_allowed(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_EIP712;
    g_field_tlv_from_apdu_ret = TLV_APDU_SUCCESS;
    assert_int_equal(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
}

// =============================================================================
// Internal handle_tlv_payload — invoked through the wrap so we can hit
// the static helpers' branches in cmd_field.c / cmd_tx_info.c.
// =============================================================================

// --- cmd_field internal handler ---

static void test_field_handler_happy_path(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_tx_ctx_is_root_ret = true;  // not signing EIP-712 & at root → exit while loop
    g_validate_instruction_hash_budget = 0;

    assert_int_equal(handle_field(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    assert_true(g_field_tlv_handler_returned);
    // cleanup_field is NOT called on happy path
    assert_int_equal(g_cleanup_field_calls, 0);
}

static void test_field_handler_handle_field_struct_failure(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_handle_field_struct_ret = false;

    handle_field(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
    // cleanup_field must be called to release any half-built state
    assert_int_equal(g_cleanup_field_calls, 1);
}

static void test_field_handler_hash_failure(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_cx_hash_ret = 0x1234;  // not CX_OK

    handle_field(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
    assert_int_equal(g_cleanup_field_calls, 1);
}

static void test_field_handler_verify_failure(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_verify_field_struct_ret = false;

    handle_field(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
    assert_int_equal(g_cleanup_field_calls, 1);
}

static void test_field_handler_format_failure(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_format_field_ret = false;

    handle_field(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
    // format_field failure does NOT trigger cleanup_field (different path)
    assert_int_equal(g_cleanup_field_calls, 0);
}

static void test_field_handler_eip712_pop_loop_runs(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_EIP712;  // forces the while loop entry
    g_field_tlv_invoke_handler = true;
    // The validator returns true twice then false → two pops, then exit.
    g_validate_instruction_hash_budget = 2;

    handle_field(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_int_equal(g_tx_ctx_pop_calls, 2);
}

static void test_field_handler_process_empty_failure_propagates(void **state) {
    (void) state;
    static const s_tx_info dummy = {0};
    g_tx_info_ret = &dummy;
    appState = APP_STATE_SIGNING_EIP712;
    g_field_tlv_invoke_handler = true;
    g_validate_instruction_hash_budget = 1;  // first iter true, then false
    g_process_empty_txs_after_ret = false;   // bail out of while loop

    handle_field(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
}

// --- cmd_tx_info internal handler ---

static void test_tx_info_handler_happy_path(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;

    assert_int_equal(handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL), SWO_SUCCESS);
    assert_true(g_field_tlv_handler_returned);
}

static void test_tx_info_handler_struct_failure(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_handle_tx_info_struct_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
}

static void test_tx_info_handler_verify_failure(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_verify_tx_info_struct_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
}

static void test_tx_info_handler_no_matching_ctx_rejected(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_find_matching_tx_ctx_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
}

static void test_tx_info_handler_set_info_failure_propagates(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    g_field_tlv_invoke_handler = true;
    g_set_tx_info_into_tx_ctx_ret = false;

    handle_tx_info(P1_FIRST_CHUNK, 0, 0, NULL);
    assert_false(g_field_tlv_handler_returned);
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_field_wrong_appstate_rejected, reset),
        cmocka_unit_test_setup(test_field_signing_message_state_rejected, reset),
        cmocka_unit_test_setup(test_field_no_tx_info_triggers_cleanup, reset),
        cmocka_unit_test_setup(test_field_tlv_failure_returns_incorrect_data, reset),
        cmocka_unit_test_setup(test_field_tlv_success_returns_success, reset),
        cmocka_unit_test_setup(test_field_signing_eip712_allowed, reset),
        cmocka_unit_test_setup(test_tx_info_wrong_appstate_rejected, reset),
        cmocka_unit_test_setup(test_tx_info_signing_message_state_rejected, reset),
        cmocka_unit_test_setup(test_tx_info_tlv_failure_returns_incorrect_data, reset),
        cmocka_unit_test_setup(test_tx_info_tlv_success_returns_success, reset),
        cmocka_unit_test_setup(test_tx_info_eip712_signing_allowed, reset),
        // Internal handler tests
        cmocka_unit_test_setup(test_field_handler_happy_path, reset),
        cmocka_unit_test_setup(test_field_handler_handle_field_struct_failure, reset),
        cmocka_unit_test_setup(test_field_handler_hash_failure, reset),
        cmocka_unit_test_setup(test_field_handler_verify_failure, reset),
        cmocka_unit_test_setup(test_field_handler_format_failure, reset),
        cmocka_unit_test_setup(test_field_handler_eip712_pop_loop_runs, reset),
        cmocka_unit_test_setup(test_field_handler_process_empty_failure_propagates, reset),
        cmocka_unit_test_setup(test_tx_info_handler_happy_path, reset),
        cmocka_unit_test_setup(test_tx_info_handler_struct_failure, reset),
        cmocka_unit_test_setup(test_tx_info_handler_verify_failure, reset),
        cmocka_unit_test_setup(test_tx_info_handler_no_matching_ctx_rejected, reset),
        cmocka_unit_test_setup(test_tx_info_handler_set_info_failure_propagates, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
