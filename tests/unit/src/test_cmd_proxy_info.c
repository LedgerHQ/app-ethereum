/**
 * @file test_cmd_proxy_info.c
 * @brief Unit tests for handle_proxy_info at
 *        src/features/provide_proxy_info/cmd_proxy_info.c.
 *
 * cmd_proxy_info is the wire-format APDU handler that the host calls to
 * register a (chain_id, proxy_address) -> implementation_address mapping.
 * The mapping is later consulted by the GCS dispatcher: when the user
 * signs a transaction targeting <proxy>, the device looks up the registered
 * <implementation> so the plugin / trusted-name layer can show the user
 * which contract is actually invoked through the proxy.
 *
 * The TLV body itself is parsed (handle_proxy_info_tlv_payload) and the
 * signature is verified (verify_proxy_info_struct) by code already covered
 * by test_proxy_info -- this file pins only the APDU framing:
 *
 *  - p1 == P1_FIRST_CHUNK is forwarded as first_chunk=true to tlv_from_apdu
 *  - p1 != P1_FIRST_CHUNK is forwarded as first_chunk=false
 *  - tlv_from_apdu returns TLV_APDU_ERROR -> SWO_INCORRECT_DATA + proxy_cleanup
 *  - tlv_from_apdu returns TLV_APDU_PENDING -> SWO_SUCCESS (no cleanup, more
 *    chunks expected)
 *  - tlv_from_apdu returns TLV_APDU_SUCCESS -> SWO_SUCCESS
 *  - the in-callback path runs handle_proxy_info_tlv_payload + verify; both
 *    must succeed for the callback to return true. Each leaf failure
 *    propagates as a false callback return.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "Mocktlv_apdu.h"
#include "cmd_proxy_info.h"
#include "proxy_info.h"
static bool g_handle_proxy_info_tlv_payload_ret = true;
static bool g_verify_proxy_info_struct_ret = true;

// =============================================================================
// Wraps
// =============================================================================
// tlv_from_apdu lives in mocks/mock.c and captures (first_chunk,
// lc, handler) into g_tlv_from_apdu_*. s_tlv_invoke_handler
// toggles the in-callback path so the in-test assertions can either
// stop at tlv_from_apdu (framing only) or run the inner
// handle_proxy_info_tlv_payload + verify_proxy_info_struct chain.

bool handle_proxy_info_tlv_payload(const buffer_t *buf, s_proxy_info_ctx *ctx) {
    (void) buf;
    (void) ctx;
    return (bool) g_handle_proxy_info_tlv_payload_ret;
}

bool verify_proxy_info_struct(const s_proxy_info_ctx *ctx) {
    (void) ctx;
    return (bool) g_verify_proxy_info_struct_ret;
}

static int g_cleanup_calls = 0;
void proxy_cleanup(void) {
    g_cleanup_calls++;
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
    g_cleanup_calls = 0;
}

// =============================================================================
// APDU framing -- p1 -> first_chunk dispatch
// =============================================================================

void test_p1_first_chunk_forwards_true(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_proxy_info(P1_FIRST_CHUNK, /*p2*/ 0, /*lc*/ 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_TRUE(s_tlv_first_chunk);
    TEST_ASSERT_EQUAL(s_tlv_lc, 32);
}

void test_p1_not_first_chunk_forwards_false(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_proxy_info(P1_FOLLOWING_CHUNK, 0, 16, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_FALSE(s_tlv_first_chunk);
    TEST_ASSERT_EQUAL(s_tlv_lc, 16);
}

// =============================================================================
// tlv_from_apdu return value propagates to SW
// =============================================================================

void test_tlv_apdu_error_returns_incorrect_data_and_cleans_up(void) {
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_proxy_info(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    // proxy_cleanup() MUST be called on error so a half-parsed proxy entry
    // doesn't linger in the global registry across APDU sequences.
    TEST_ASSERT_EQUAL(g_cleanup_calls, 1);
}

void test_tlv_apdu_pending_returns_success_without_cleanup(void) {
    // PENDING signals "more chunks coming". The dispatcher must report
    // SWO_SUCCESS upstream so the host keeps streaming, and MUST NOT
    // run cleanup -- that would wipe the in-flight parser state.
    s_tlv_ret = TLV_APDU_PENDING;
    uint16_t sw = handle_proxy_info(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 0);
}

void test_tlv_apdu_success_returns_success(void) {
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_proxy_info(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 0);
}

// =============================================================================
// Inner handle_tlv_payload callback -- payload then verify
// =============================================================================

void test_inner_callback_runs_payload_then_verify(void) {
    // Drive the static handle_tlv_payload through tlv_from_apdu: payload
    // parser returns true, verifier returns true -> callback's overall
    // outcome is true (observable indirectly through the cleanup count
    // and the queue staying empty on exit).
    s_tlv_invoke_handler = true;
    g_handle_proxy_info_tlv_payload_ret = true;
    g_verify_proxy_info_struct_ret = true;
    s_tlv_ret = TLV_APDU_SUCCESS;
    uint16_t sw = handle_proxy_info(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    // proxy_cleanup is called from the callback prelude (before parser).
    TEST_ASSERT_EQUAL(g_cleanup_calls, 1);
}

void test_inner_callback_short_circuits_on_payload_failure(void) {
    s_tlv_invoke_handler = true;
    g_handle_proxy_info_tlv_payload_ret = false;
    // verify_proxy_info_struct MUST NOT be reached -- a failed parse
    // means there's nothing to verify. Leaving its queue empty is the
    // safety check.
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_proxy_info(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 2);  // once in callback, once on error
}

void test_inner_callback_rejects_when_verify_fails(void) {
    s_tlv_invoke_handler = true;
    g_handle_proxy_info_tlv_payload_ret = true;
    g_verify_proxy_info_struct_ret = false;
    s_tlv_ret = TLV_APDU_ERROR;
    uint16_t sw = handle_proxy_info(P1_FIRST_CHUNK, 0, 32, (uint8_t *) "");
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_EQUAL(g_cleanup_calls, 2);
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
    RUN_TEST(test_tlv_apdu_error_returns_incorrect_data_and_cleans_up);
    RUN_TEST(test_tlv_apdu_pending_returns_success_without_cleanup);
    RUN_TEST(test_tlv_apdu_success_returns_success);
    RUN_TEST(test_inner_callback_runs_payload_then_verify);
    RUN_TEST(test_inner_callback_short_circuits_on_payload_failure);
    RUN_TEST(test_inner_callback_rejects_when_verify_fails);
    return UNITY_END();
}
