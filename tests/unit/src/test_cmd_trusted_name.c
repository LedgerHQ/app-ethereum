/**
 * @file test_cmd_trusted_name.c
 * @brief Unit tests for handle_trusted_name at
 *        src/features/provide_trusted_name/cmd_trusted_name.c.
 *
 * cmd_trusted_name is the APDU framing around the trusted-name registry
 * (TRUSTED_NAME APDU). The host streams a TLV blob (one or more chunks);
 * the device parses it via handle_trusted_name_tlv_payload, verifies the
 * Ledger backend signature via verify_trusted_name_struct, and finally
 * advances the per-session challenge nonce via roll_challenge so a
 * replay of the same payload is rejected.
 *
 * The TLV parser and verifier are pinned by test_trusted_name. This file
 * only covers the APDU framing:
 *
 *  - p1 == P1_FIRST_CHUNK -> first_chunk=true forwarded to tlv_from_apdu
 *  - p1 != P1_FIRST_CHUNK -> first_chunk=false
 *  - tlv_from_apdu returns TLV_APDU_ERROR   -> SWO_INCORRECT_DATA
 *  - tlv_from_apdu returns TLV_APDU_PENDING -> SWO_SUCCESS (more chunks)
 *  - tlv_from_apdu returns TLV_APDU_SUCCESS -> SWO_SUCCESS
 *  - inner callback path runs parser + verifier; roll_challenge MUST be
 *    called after verify regardless of its outcome (CWE-294 anti-replay).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "Mocktlv_apdu.h"
#include "cmd_trusted_name.h"
#include "trusted_name.h"
static bool g_handle_trusted_name_tlv_payload_ret = true;
static bool g_verify_trusted_name_struct_ret = true;

// =============================================================================
// Wraps
// =============================================================================

bool handle_trusted_name_tlv_payload(const buffer_t *buf, s_trusted_name_ctx *ctx) {
    (void) buf;
    (void) ctx;
    return (bool) g_handle_trusted_name_tlv_payload_ret;
}

bool verify_trusted_name_struct(const s_trusted_name_ctx *ctx) {
    (void) ctx;
    return (bool) g_verify_trusted_name_struct_ret;
}

static int g_roll_calls = 0;
void roll_challenge(void) {
    g_roll_calls++;
}

// =============================================================================
// Local tlv_from_apdu stub
// =============================================================================

static bool s_tlv_first_chunk = false;
static uint8_t s_tlv_lc = 0;
static bool s_tlv_invoke_handler = false;
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
    if (s_tlv_invoke_handler && handler != NULL) {
        buffer_t buf = {.ptr = NULL, .size = 0, .offset = 0};
        (void) handler(&buf);
    }
    return s_tlv_ret;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    s_tlv_invoke_handler = false;
    s_tlv_first_chunk = false;
    s_tlv_lc = 0;
    s_tlv_ret = TLV_APDU_SUCCESS;
    g_roll_calls = 0;
}

// =============================================================================
// APDU framing -- p1 -> first_chunk dispatch
// =============================================================================

void test_p1_first_chunk_forwards_true(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_trusted_name(P1_FIRST_CHUNK, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_TRUE(s_tlv_first_chunk);
    TEST_ASSERT_EQUAL(s_tlv_lc, 32);
}

void test_p1_not_first_chunk_forwards_false(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_trusted_name(P1_FOLLOWING_CHUNK, (uint8_t *) "", 16);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_FALSE(s_tlv_first_chunk);
}

// =============================================================================
// tlv_from_apdu return value propagates to SW
// =============================================================================

void test_tlv_apdu_error_returns_incorrect_data(void) {
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_trusted_name(P1_FIRST_CHUNK, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_tlv_apdu_pending_returns_success(void) {
    s_tlv_ret = TLV_APDU_PENDING;
    uint16_t sw = handle_trusted_name(P1_FIRST_CHUNK, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
}

void test_tlv_apdu_success_returns_success(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_trusted_name(P1_FIRST_CHUNK, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
}

// =============================================================================
// Inner handle_tlv_payload callback
// =============================================================================

void test_inner_callback_runs_payload_then_verify_then_roll(void) {
    s_tlv_invoke_handler = true;
    g_handle_trusted_name_tlv_payload_ret = true;
    g_verify_trusted_name_struct_ret = true;
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_trusted_name(P1_FIRST_CHUNK, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    // roll_challenge MUST be called after verify on the success path to
    // burn the challenge nonce -- otherwise the host could replay the
    // same signed payload across sessions (CWE-294).
    TEST_ASSERT_EQUAL(g_roll_calls, 1);
}

void test_inner_callback_rolls_challenge_even_on_verify_failure(void) {
    // CRITICAL: the design rolls the challenge AFTER verify regardless of
    // the result, so a brute-force attacker can't probe many candidate
    // signatures against the same nonce. If a future refactor short-
    // circuits roll_challenge on verify-failure, this test catches it.
    s_tlv_invoke_handler = true;
    g_handle_trusted_name_tlv_payload_ret = true;
    g_verify_trusted_name_struct_ret = false;
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_trusted_name(P1_FIRST_CHUNK, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_EQUAL(g_roll_calls, 1);
}

void test_inner_callback_short_circuits_on_payload_failure(void) {
    // Parser failure short-circuits BEFORE verify, so roll_challenge MUST
    // NOT fire here -- the challenge can still be used on the next
    // genuine attempt.
    s_tlv_invoke_handler = true;
    g_handle_trusted_name_tlv_payload_ret = false;
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_trusted_name(P1_FIRST_CHUNK, (uint8_t *) "", 32);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_EQUAL(g_roll_calls, 0);
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
    RUN_TEST(test_p1_first_chunk_forwards_true);
    RUN_TEST(test_p1_not_first_chunk_forwards_false);
    RUN_TEST(test_tlv_apdu_error_returns_incorrect_data);
    RUN_TEST(test_tlv_apdu_pending_returns_success);
    RUN_TEST(test_tlv_apdu_success_returns_success);
    RUN_TEST(test_inner_callback_runs_payload_then_verify_then_roll);
    RUN_TEST(test_inner_callback_rolls_challenge_even_on_verify_failure);
    RUN_TEST(test_inner_callback_short_circuits_on_payload_failure);
    return UNITY_END();
}
