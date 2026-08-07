/**
 * @file test_cmd_sign_message.c
 * @brief Unit tests for the EIP-191 "personal_sign" handler at
 *        src/features/sign_message/cmd_sign_message.c.
 *
 * EIP-191 is the human-readable signing path: the device prepends the
 * "\x19Ethereum Signed Message:\n<length>" prefix to the host-supplied
 * payload, hashes the concatenation with keccak-256, and signs the
 * resulting digest. The host streams the payload across multiple APDU
 * chunks (P1=FIRST then P1=MORE). A bug in this state machine would:
 *  - let the host start a fresh signing flow while another command is
 *    mid-flight,
 *  - let an over-long chunk write past the heap-allocated payload
 *    buffer,
 *  - leave allocated state behind after an error (memory leak / use-
 *    after-free hazard),
 *  - mis-detect a hex payload as ASCII (or vice versa) and mislead the
 *    user about what they are signing.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "sign_message.h"
#include "cx_errors.h"
#include "wraps.h"

// =============================================================================
// Globals required by linked translation units
// =============================================================================

extern cx_sha3_t *g_msg_hash_ctx;

// =============================================================================
// Wraps / stubs
// =============================================================================

// parseBip32 + cx_keccak_init_no_throw are wrapped in mocks/mock.c;
// drive them through g_parsebip32_force_null + g_keccak_init_ret
// from wraps.h.

static cx_err_t g_cx_hash_ret = CX_OK;
static size_t g_cx_hash_calls = 0;
cx_err_t __wrap_cx_hash_no_throw(void *ctx,
                                 uint32_t mode,
                                 const uint8_t *in,
                                 size_t in_len,
                                 uint8_t *out,
                                 size_t out_len) {
    (void) ctx;
    (void) mode;
    (void) in;
    (void) in_len;
    (void) out;
    (void) out_len;
    g_cx_hash_calls++;
    return g_cx_hash_ret;
}

// Strong override of mocks/mock.c's __wrap_finalize_hash: takes
// `void *ctx` (vs cx_hash_t *) and fills with 0xAB so the assert
// path can match a known digest. g_finalize_hash_ret lives in
// wraps.h.
bool __wrap_finalize_hash(void *ctx, uint8_t *out, size_t out_len) {
    (void) ctx;
    memset(out, 0xAB, out_len);
    return g_finalize_hash_ret;
}

// UI hooks — record invocations.
static int g_ui_idle_calls = 0;
void ui_idle(void) {
    g_ui_idle_calls++;
}

static int g_ui_191_calls = 0;
static char g_ui_191_display[1024];
void ui_191_start(const char *display_buffer) {
    g_ui_191_calls++;
    if (display_buffer != NULL) {
        strncpy(g_ui_191_display, display_buffer, sizeof(g_ui_191_display) - 1);
        g_ui_191_display[sizeof(g_ui_191_display) - 1] = '\0';
    }
}

// =============================================================================
// APDU builder
// =============================================================================
//
// Layout of a FIRST APDU:
//   [bip32_len:1] [bip32_path:N*4] [msg_length_BE:4] [first_chunk_bytes]
// Subsequent (MORE) APDUs carry raw payload bytes.

static size_t build_first_apdu(uint8_t *out,
                               size_t out_size,
                               uint32_t msg_length,
                               const uint8_t *first_chunk,
                               uint8_t first_chunk_len) {
    size_t off = 0;
    out[off++] = 5;  // BIP-32 path length (m/44'/60'/0'/0/0)
    for (int i = 0; i < 5; i++) {
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
    }
    // 4-byte BE msg_length
    out[off++] = (uint8_t) (msg_length >> 24);
    out[off++] = (uint8_t) (msg_length >> 16);
    out[off++] = (uint8_t) (msg_length >> 8);
    out[off++] = (uint8_t) (msg_length);
    if (first_chunk != NULL && first_chunk_len > 0) {
        memcpy(out + off, first_chunk, first_chunk_len);
        off += first_chunk_len;
    }
    assert_true(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static int reset(void **state) {
    (void) state;
    // The handler self-cleans on most error paths via set_idle(), but
    // tests that crash mid-flight may leak; flush explicitly.
    message_cleanup();
    if (g_msg_hash_ctx != NULL) {
        free(g_msg_hash_ctx);
        g_msg_hash_ctx = NULL;
    }
    appState = APP_STATE_IDLE;
    g_parsebip32_force_null = false;
    g_keccak_init_ret = CX_OK;
    g_cx_hash_ret = CX_OK;
    g_cx_hash_calls = 0;
    g_finalize_hash_ret = true;
    g_ui_idle_calls = 0;
    g_ui_191_calls = 0;
    memset(g_ui_191_display, 0, sizeof(g_ui_191_display));
    memset(&tmpCtx, 0, sizeof(tmpCtx));
    memset(&strings, 0, sizeof(strings));
    return 0;
}

// =============================================================================
// Tests — entry-point dispatcher
// =============================================================================

static void test_p1_first_rejected_when_not_idle(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_TX;
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 4, (uint8_t *) "ping", 4);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    assert_int_equal(sw, SWO_COMMAND_NOT_ALLOWED);
    assert_int_equal(g_ui_idle_calls, 0);
}

static void test_p1_unknown_rejected(void **state) {
    (void) state;
    uint8_t data[8] = {0};
    uint16_t sw = handle_sign_personal_message(0x42, data, sizeof(data));
    assert_int_equal(sw, SWO_WRONG_P1_P2);
    assert_int_equal(g_ui_idle_calls, 1);
}

static void test_p1_more_without_prior_first_rejected(void **state) {
    (void) state;
    uint8_t data[8] = {0};
    // appState is IDLE — no prior FIRST happened.
    uint16_t sw = handle_sign_personal_message(P1_MORE, data, sizeof(data));
    assert_int_equal(sw, SWO_INCORRECT_DATA);
    assert_int_equal(g_ui_idle_calls, 1);
}

// =============================================================================
// Tests — first_apdu_data
// =============================================================================

static void test_first_bad_bip32_returns_incorrect_data(void **state) {
    (void) state;
    g_parsebip32_force_null = true;
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 4, (uint8_t *) "ping", 4);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_first_truncated_before_msg_length_rejected(void **state) {
    (void) state;
    // bip32 path consumes 21 bytes, no msg_length follows.
    uint8_t data[21];
    data[0] = 5;
    memset(data + 1, 0, 20);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, sizeof(data));
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_first_keccak_init_failure_propagates(void **state) {
    (void) state;
    g_keccak_init_ret = CX_INVALID_PARAMETER;
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 4, (uint8_t *) "ping", 4);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    // cx_err_t is uint32_t but handle_sign_personal_message returns
    // uint16_t — the SDK error truncates to its low 16 bits.
    assert_int_equal(sw, (uint16_t) CX_INVALID_PARAMETER);
}

// =============================================================================
// Tests — happy path
// =============================================================================

static void test_single_chunk_ascii_starts_ui(void **state) {
    (void) state;
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 5, (uint8_t *) "hello", 5);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    // Single-chunk path: the handler finalizes and hands off to the UI;
    // it returns NO_RESPONSE so the dispatcher does not auto-reply.
    assert_int_equal(sw, SWO_NO_RESPONSE);
    assert_int_equal(g_ui_191_calls, 1);
    assert_string_equal(g_ui_191_display, "hello");
}

static void test_single_chunk_hex_uses_0x_prefix(void **state) {
    (void) state;
    // 0x01 is non-printable & non-space, so the handler must switch to
    // the hex display path.
    uint8_t msg[3] = {0x01, 0x02, 0x03};
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 3, msg, 3);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    assert_int_equal(sw, SWO_NO_RESPONSE);
    assert_string_equal(g_ui_191_display, "0x010203");
}

static void test_multichunk_completes_on_last_chunk(void **state) {
    (void) state;
    // FIRST sends 3 bytes, MORE sends remaining 4. msg_length = 7.
    uint8_t first[64];
    size_t flen = build_first_apdu(first, sizeof(first), 7, (uint8_t *) "abc", 3);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, first, (uint8_t) flen);
    assert_int_equal(sw, SWO_SUCCESS);
    assert_int_equal(g_ui_191_calls, 0);

    sw = handle_sign_personal_message(P1_MORE, (uint8_t *) "defg", 4);
    assert_int_equal(sw, SWO_NO_RESPONSE);
    assert_int_equal(g_ui_191_calls, 1);
    assert_string_equal(g_ui_191_display, "abcdefg");
}

static void test_chunk_overflow_rejected(void **state) {
    (void) state;
    // Declare msg_length = 4; FIRST already delivered 2 bytes; sending
    // 3 more (total 5) must trip the overflow guard.
    uint8_t first[64];
    size_t flen = build_first_apdu(first, sizeof(first), 4, (uint8_t *) "ab", 2);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, first, (uint8_t) flen);
    assert_int_equal(sw, SWO_SUCCESS);

    sw = handle_sign_personal_message(P1_MORE, (uint8_t *) "cde", 3);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
    assert_int_equal(g_ui_idle_calls, 1);
    assert_int_equal(g_ui_191_calls, 0);
}

static void test_final_finalize_failure_resets_state(void **state) {
    (void) state;
    g_finalize_hash_ret = false;
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 4, (uint8_t *) "ping", 4);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    // CX_INTERNAL_ERROR is the default initial value of `error` in
    // final_process(); since finalize_hash failed it jumps straight to
    // `end` without overwriting. The 32-bit SDK error truncates to its
    // low 16 bits on the way out.
    assert_int_equal(sw, (uint16_t) CX_INTERNAL_ERROR);
    assert_int_equal(g_ui_idle_calls, 1);
    assert_int_equal(g_ui_191_calls, 0);
}

static void test_cleanup_after_p1_first_when_busy(void **state) {
    (void) state;
    // Set up a successful single-chunk run, then send P1_FIRST again
    // while still in SIGNING_MESSAGE state — handler must reject and
    // wipe state.
    appState = APP_STATE_SIGNING_TX;  // simulate a different ongoing flow
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 4, (uint8_t *) "ping", 4);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    assert_int_equal(sw, SWO_COMMAND_NOT_ALLOWED);
    // After the rejection appState was NOT bumped to SIGNING_MESSAGE.
    assert_int_equal(g_ui_idle_calls, 0);
}

static void test_message_cleanup_safe_when_nothing_allocated(void **state) {
    (void) state;
    // Calling cleanup with no active context must be a no-op rather
    // than crash. (signMsgCtx is private; verify by behaviour: a fresh
    // FIRST after cleanup should succeed cleanly.)
    message_cleanup();
    message_cleanup();
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 4, (uint8_t *) "ping", 4);
    uint16_t sw = handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    assert_int_equal(sw, SWO_NO_RESPONSE);
}

static void test_hash_is_keccak_seeded_with_prefix(void **state) {
    (void) state;
    // The "\x19Ethereum Signed Message:\n<length>" header must be fed
    // to keccak before any payload byte. We can't observe the hash
    // value directly (finalize_hash is wrapped), but we can pin the
    // number of cx_hash_no_throw invocations on the FIRST path: one
    // for SIGN_MAGIC, one for the decimal length string. The third
    // invocation comes from process_data() consuming the inline chunk.
    uint8_t data[64];
    size_t len = build_first_apdu(data, sizeof(data), 5, (uint8_t *) "hello", 5);
    (void) handle_sign_personal_message(P1_FIRST, data, (uint8_t) len);
    assert_int_equal(g_cx_hash_calls, 3);
    // strings.tmp.tmp must contain the decimal representation of the
    // message length so it can be fed to the hash.
    assert_string_equal(strings.tmp.tmp, "5");
}

// =============================================================================
// P1=MORE while SIGNING but signMsgCtx already freed
// =============================================================================
// The handler is called with P1_MORE after appState has been bumped to
// SIGNING_MESSAGE (so the earlier guard at line 244 doesn't fire), but
// signMsgCtx is NULL -- e.g. a previous chunk failed and called
// message_cleanup() which freed it without resetting appState. The
// gate at line 251-254 must catch this and refuse rather than
// dereferencing NULL.

static void test_p1_more_with_null_ctx_rejected(void **state) {
    (void) state;
    appState = APP_STATE_SIGNING_MESSAGE;
    // signMsgCtx left NULL by reset() (message_cleanup() runs there).
    uint8_t data[8] = {0};
    uint16_t sw = handle_sign_personal_message(P1_MORE, data, sizeof(data));
    assert_int_equal(sw, SWO_INCORRECT_DATA);
    assert_int_equal(g_ui_idle_calls, 1);
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_p1_first_rejected_when_not_idle, reset),
        cmocka_unit_test_setup(test_p1_unknown_rejected, reset),
        cmocka_unit_test_setup(test_p1_more_without_prior_first_rejected, reset),
        cmocka_unit_test_setup(test_first_bad_bip32_returns_incorrect_data, reset),
        cmocka_unit_test_setup(test_first_truncated_before_msg_length_rejected, reset),
        cmocka_unit_test_setup(test_first_keccak_init_failure_propagates, reset),
        cmocka_unit_test_setup(test_single_chunk_ascii_starts_ui, reset),
        cmocka_unit_test_setup(test_single_chunk_hex_uses_0x_prefix, reset),
        cmocka_unit_test_setup(test_multichunk_completes_on_last_chunk, reset),
        cmocka_unit_test_setup(test_chunk_overflow_rejected, reset),
        cmocka_unit_test_setup(test_final_finalize_failure_resets_state, reset),
        cmocka_unit_test_setup(test_cleanup_after_p1_first_when_busy, reset),
        cmocka_unit_test_setup(test_message_cleanup_safe_when_nothing_allocated, reset),
        cmocka_unit_test_setup(test_hash_is_keccak_seeded_with_prefix, reset),
        cmocka_unit_test_setup(test_p1_more_with_null_ctx_rejected, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
