/**
 * @file test_signature_check.c
 * @brief Unit tests for check_signature_with_pubkey at src/utils/signature_check.c.
 *
 * check_signature_with_pubkey is THE crypto gate behind every host-supplied
 * descriptor that affects what the user sees on screen: trusted-name
 * resolution, proxy-info, safe-account, plugin registrations, gating
 * payloads, network metadata, tx-simulation descriptors. A regression here
 * is a CWE-347 (improper signature verification) by silent failure --
 * fake "Uniswap.eth" / fake plugin name / fake risk score reaches the user.
 *
 * The function is a thin dispatcher:
 *
 *   check_signature_with_pki(...) -> status_t
 *     CHECK_SIGNATURE_WITH_PKI_SUCCESS                  -> return true
 *     CHECK_SIGNATURE_WITH_PKI_MISSING_CERTIFICATE      -> return false
 *     CHECK_SIGNATURE_WITH_PKI_WRONG_CERTIFICATE_USAGE  -> return false
 *     CHECK_SIGNATURE_WITH_PKI_WRONG_CERTIFICATE_CURVE  -> return false
 *     CHECK_SIGNATURE_WITH_PKI_WRONG_SIGNATURE          -> return false
 *     (default)                                         -> return false
 *
 * Tests pin every status_t branch. The SDK lib_pki/ledger_pki.c is NOT
 * linked into this target -- we provide check_signature_with_pki directly
 * so each test can drive the outcome.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "ledger_pki.h"       // SDK lib_pki header: status enum + check_signature_with_pki proto
#include "signature_check.h"  // check_signature_with_pubkey prototype

static int g_check_signature_with_pki_ret = 0;

// =============================================================================
// Stubs / wraps
// =============================================================================

// SDK lib_pki entry point. Each test sets its desired return value.
check_signature_with_pki_status_t check_signature_with_pki(const buffer_t hash,
                                                           const uint8_t *expected_key_usage,
                                                           const cx_curve_t *expected_curve,
                                                           const buffer_t signature) {
    (void) hash;
    (void) expected_key_usage;
    (void) expected_curve;
    (void) signature;
    return (check_signature_with_pki_status_t) g_check_signature_with_pki_ret;
}

// =============================================================================
// Test fixtures -- valid-looking arguments. Bodies don't matter; the stub
// ignores them. Sized to plausibly-real values (32-byte hash, DER-encoded
// ECDSA signature ~71 bytes) so the function-under-test sees the same shape
// it would on-device.
// =============================================================================

static uint8_t g_hash[32];
static uint8_t g_sig[71];

#define KEY_USAGE_TRUSTED_NAME 0x01

static void reset(void) {
    memset(g_hash, 0xAA, sizeof(g_hash));
    memset(g_sig, 0xCC, sizeof(g_sig));
}

static bool call_check(void) {
    return check_signature_with_pubkey(g_hash,
                                       sizeof(g_hash),
                                       KEY_USAGE_TRUSTED_NAME,
                                       g_sig,
                                       sizeof(g_sig));
}

// =============================================================================
// PKI statuses
// =============================================================================

void test_pki_success_returns_true(void) {
    g_check_signature_with_pki_ret = CHECK_SIGNATURE_WITH_PKI_SUCCESS;
    TEST_ASSERT_TRUE(call_check());
}

void test_pki_missing_cert_returns_false(void) {
    g_check_signature_with_pki_ret = CHECK_SIGNATURE_WITH_PKI_MISSING_CERTIFICATE;
    TEST_ASSERT_FALSE(call_check());
}

void test_pki_wrong_usage_returns_false(void) {
    g_check_signature_with_pki_ret = CHECK_SIGNATURE_WITH_PKI_WRONG_CERTIFICATE_USAGE;
    TEST_ASSERT_FALSE(call_check());
}

void test_pki_wrong_curve_returns_false(void) {
    g_check_signature_with_pki_ret = CHECK_SIGNATURE_WITH_PKI_WRONG_CERTIFICATE_CURVE;
    TEST_ASSERT_FALSE(call_check());
}

void test_pki_wrong_signature_returns_false(void) {
    g_check_signature_with_pki_ret = CHECK_SIGNATURE_WITH_PKI_WRONG_SIGNATURE;
    TEST_ASSERT_FALSE(call_check());
}

void test_pki_unknown_status_returns_false(void) {
    // Push a value outside the defined enum range to exercise the `default:`
    // fall-through. The function MUST fail closed.
    g_check_signature_with_pki_ret = 0xFF;
    TEST_ASSERT_FALSE(call_check());
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pki_success_returns_true);
    RUN_TEST(test_pki_missing_cert_returns_false);
    RUN_TEST(test_pki_wrong_usage_returns_false);
    RUN_TEST(test_pki_wrong_curve_returns_false);
    RUN_TEST(test_pki_wrong_signature_returns_false);
    RUN_TEST(test_pki_unknown_status_returns_false);
    return UNITY_END();
}
