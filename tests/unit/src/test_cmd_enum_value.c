/**
 * @file test_cmd_enum_value.c
 * @brief Unit tests for handle_enum_value at
 *        src/features/provide_enum_value/cmd_enum_value.c.
 *
 * cmd_enum_value is the APDU framing around the enum-value registry
 * (PROVIDE_ENUM_VALUE APDU). The host streams a TLV-encoded mapping
 * `(chain_id, contract, selector, enum_id, value) -> display string` --
 * what the user actually sees when the contract returns an integer status
 * (e.g. AAVE health-factor band, Compound interest-rate mode). A bug
 * here lets the host display arbitrary strings against a real enum tag.
 *
 * The TLV parser + verifier are pinned by test_enum_value. This file
 * only covers the APDU framing:
 *
 *  - p1 == P1_FIRST_CHUNK -> first_chunk=true forwarded to tlv_from_apdu
 *  - p1 != P1_FIRST_CHUNK -> first_chunk=false
 *  - tlv_from_apdu returns TLV_APDU_ERROR   -> SWO_INCORRECT_DATA
 *  - tlv_from_apdu returns TLV_APDU_PENDING -> SWO_SUCCESS (more chunks)
 *  - tlv_from_apdu returns TLV_APDU_SUCCESS -> SWO_SUCCESS
 *  - inner callback path: payload-parse failure short-circuits before
 *    verify; verify failure also returns false.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "Mocktlv_apdu.h"
#include "cmd_enum_value.h"
#include "enum_value.h"
static bool g_handle_enum_value_tlv_payload_ret = true;
static bool g_verify_enum_value_struct_ret = true;

// =============================================================================
// Wraps
// =============================================================================

bool handle_enum_value_tlv_payload(const buffer_t *buf, s_enum_value_ctx *ctx) {
    (void) buf;
    (void) ctx;
    return (bool) g_handle_enum_value_tlv_payload_ret;
}

bool verify_enum_value_struct(const s_enum_value_ctx *ctx) {
    (void) ctx;
    return (bool) g_verify_enum_value_struct_ret;
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
}

// =============================================================================
// APDU framing
// =============================================================================

void test_p1_first_chunk_forwards_true(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_enum_value(P1_FIRST_CHUNK, /*p2*/ 0, /*lc*/ 24, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_TRUE(s_tlv_first_chunk);
    TEST_ASSERT_EQUAL(s_tlv_lc, 24);
}

void test_p1_not_first_chunk_forwards_false(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_enum_value(P1_FOLLOWING_CHUNK, 0, 16, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_FALSE(s_tlv_first_chunk);
}

// =============================================================================
// tlv_from_apdu return value propagates to SW
// =============================================================================

void test_tlv_apdu_error_returns_incorrect_data(void) {
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_enum_value(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_tlv_apdu_pending_returns_success(void) {
    s_tlv_ret = TLV_APDU_PENDING;
    uint16_t sw = handle_enum_value(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
}

void test_tlv_apdu_success_returns_success(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_enum_value(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
}

// =============================================================================
// Inner handle_tlv_payload callback
// =============================================================================

void test_inner_callback_runs_payload_then_verify(void) {
    s_tlv_invoke_handler = true;
    g_handle_enum_value_tlv_payload_ret = true;
    g_verify_enum_value_struct_ret = true;
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_enum_value(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
}

void test_inner_callback_short_circuits_on_payload_failure(void) {
    s_tlv_invoke_handler = true;
    g_handle_enum_value_tlv_payload_ret = false;
    // verify_enum_value_struct MUST NOT be reached.
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_enum_value(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_inner_callback_rejects_when_verify_fails(void) {
    s_tlv_invoke_handler = true;
    g_handle_enum_value_tlv_payload_ret = true;
    g_verify_enum_value_struct_ret = false;
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_enum_value(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
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
    RUN_TEST(test_inner_callback_runs_payload_then_verify);
    RUN_TEST(test_inner_callback_short_circuits_on_payload_failure);
    RUN_TEST(test_inner_callback_rejects_when_verify_fails);
    return UNITY_END();
}
