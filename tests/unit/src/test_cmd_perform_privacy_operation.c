/**
 * @file test_cmd_perform_privacy_operation.c
 * @brief Unit tests for the PERFORM_PRIVACY_OPERATION handler at
 *        src/features/perform_privacy_operation/cmd_perform_privacy_operation.c.
 *
 * PERFORM_PRIVACY_OPERATION exposes two flows behind one APDU:
 *   - P2=0x00 (PUBLIC_ENCRYPTION_KEY): derive the X25519 public
 *     encryption key for a given BIP-32 path,
 *   - P2=0x01 (SHARED_SECRET): compute the X25519 shared secret with
 *     a host-supplied peer public key (32 bytes).
 *
 * The shared-secret path is the high-impact one: it releases
 * cryptographic material derived from the device-held private key.
 * The source enforces a CWE-200 guard requiring user confirmation
 * (P1_CONFIRM) before any shared secret can leave the device, and a
 * CWE-312 scrub (explicit_bzero) on the result buffer source after
 * copy. Pin both, plus every wire-format gate.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_perform_privacy_operation.h"
// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// parseBip32 stub: set s_parsebip32_force_null to drive negative tests.
static bool s_parsebip32_force_null = false;
const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    (void) bip32;
    if (s_parsebip32_force_null) return NULL;
    if (*dataLength < 1) return NULL;
    uint8_t count = *dataBuffer;
    if ((size_t) *dataLength < 1 + (size_t) count * 4) return NULL;
    dataBuffer += 1 + count * 4;
    *dataLength -= 1 + count * 4;
    return dataBuffer;
}

// os_derive_bip32_no_throw is a static inline that delegates to
// os_derive_bip32_with_seed_no_throw, which itself wraps the syscall
// os_perso_derive_node_with_seed_key inside BEGIN_TRY/TRY. The
// syscall is the one we can wrap at link time.
static int g_derive_should_throw = 0;
void os_perso_derive_node_with_seed_key(unsigned int mode,
                                        cx_curve_t curve,
                                        const unsigned int *path,
                                        unsigned int path_len,
                                        unsigned char *privateKey,
                                        unsigned char *chain,
                                        unsigned char *seed_key,
                                        unsigned int seed_key_length) {
    (void) mode;
    (void) curve;
    (void) path;
    (void) path_len;
    (void) seed_key;
    (void) seed_key_length;
    if (privateKey != NULL) memset(privateKey, 0x11, 64);
    if (chain != NULL) memset(chain, 0xCC, 32);
    // The BEGIN_TRY/TRY block treats a longjmp inside the body as the
    // derivation failure path. We don't drive that here (see cmd_set_plugin
    // for the accepted limitation around the SDK try-context layout).
    (void) g_derive_should_throw;
}

static cx_err_t g_init_priv_ret = CX_OK;
cx_err_t cx_ecfp_init_private_key_no_throw(cx_curve_t curve,
                                           const uint8_t *rawkey,
                                           size_t key_len,
                                           cx_ecfp_private_key_t *key) {
    (void) curve;
    (void) rawkey;
    (void) key_len;
    if (key != NULL) memset(key, 0x77, sizeof(cx_ecfp_private_key_t));
    return g_init_priv_ret;
}

static cx_err_t g_gen_pair_ret = CX_OK;
cx_err_t cx_ecfp_generate_pair_no_throw(cx_curve_t curve,
                                        cx_ecfp_public_key_t *pubkey,
                                        cx_ecfp_private_key_t *privkey,
                                        bool keepprivate) {
    (void) curve;
    (void) privkey;
    (void) keepprivate;
    if (pubkey != NULL) {
        memset(&pubkey->W[0], 0x04, 1);  // uncompressed marker
        memset(&pubkey->W[1], 0x33, 64);
        pubkey->W_len = 65;
    }
    return g_gen_pair_ret;
}

static cx_err_t g_x25519_ret = CX_OK;
// Scalar the handler passed in, so a test can assert which of the two encodings
// (raw derived key vs decode_scalar'd) reaches the exchange.
static uint8_t g_x25519_scalar[32];
// Emit an all-zero result, as X25519 does for a peer point of small order.
static bool g_x25519_zero_output = false;
cx_err_t cx_x25519(uint8_t *p, const uint8_t *s, size_t s_len) {
    if ((s != NULL) && (s_len == sizeof(g_x25519_scalar))) {
        memcpy(g_x25519_scalar, s, sizeof(g_x25519_scalar));
    }
    if (p != NULL) {
        // Overwrite the host-supplied peer key with deterministic
        // shared-secret bytes so the response buffer can be inspected.
        memset(p, g_x25519_zero_output ? 0x00 : 0x99, 32);
    }
    return g_x25519_ret;
}

void __wrap_getEthAddressStringFromRawKey(const uint8_t *publicKey, char *out, uint64_t chain_id) {
    (void) publicKey;
    (void) chain_id;
    memset(out, 'A', 40);
    out[40] = '\0';
}

static int g_ui_pubkey_calls = 0;
static int g_ui_shared_calls = 0;
void ui_display_privacy_public_key(void) {
    g_ui_pubkey_calls++;
}
void ui_display_privacy_shared_secret(void) {
    g_ui_shared_calls++;
}

// =============================================================================
// APDU builder
// =============================================================================

static size_t build_apdu(uint8_t *out, size_t out_size, bool include_peer_key) {
    size_t off = 0;
    out[off++] = 5;  // BIP-32 path length
    for (int i = 0; i < 5; i++) {
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
    }
    if (include_peer_key) {
        memset(out + off, 0x55, 32);
        off += 32;
    }
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    appState = APP_STATE_IDLE;
    s_parsebip32_force_null = false;
    g_init_priv_ret = CX_OK;
    g_gen_pair_ret = CX_OK;
    g_x25519_ret = CX_OK;
    g_x25519_zero_output = false;
    memset(g_x25519_scalar, 0, sizeof(g_x25519_scalar));
    g_ui_pubkey_calls = 0;
    g_ui_shared_calls = 0;
    memset(&tmpCtx, 0, sizeof(tmpCtx));
    memset(&strings, 0, sizeof(strings));
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
}

// =============================================================================
// Tests — dispatcher guards
// =============================================================================

void test_invalid_p1_rejected(void) {
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(/*p1=*/0x05, 0, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

void test_invalid_p2_rejected(void) {
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw =
        handle_perform_privacy_operation(P1_NON_CONFIRM, /*p2=*/0x05, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

void test_shared_secret_requires_confirmation(void) {
    // CWE-200 guard: shared-secret export must require P1_CONFIRM.
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM,
                                                   /*p2=*/0x01,  // P2_SHARED_SECRET
                                                   apdu,
                                                   (uint8_t) len,
                                                   &tx);
    TEST_ASSERT_EQUAL(sw, SWO_CONDITIONS_NOT_SATISFIED);
    // No UI / no key material released.
    TEST_ASSERT_EQUAL(g_ui_pubkey_calls, 0);
    TEST_ASSERT_EQUAL(g_ui_shared_calls, 0);
}

void test_bad_bip32_rejected(void) {
    s_parsebip32_force_null = true;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_shared_secret_short_peer_key_rejected(void) {
    // P2=SHARED_SECRET requires the peer public key (32 bytes) right
    // after the BIP-32 path.
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);  // no 32-byte key
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0x01, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_DATA_LENGTH);
}

// =============================================================================
// Tests — derivation / SDK failures propagate
// =============================================================================

void test_init_private_key_failure_propagates(void) {
    g_init_priv_ret = CX_INVALID_PARAMETER;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, (uint16_t) CX_INVALID_PARAMETER);
}

// =============================================================================
// Tests — happy paths
// =============================================================================

void test_public_encryption_key_non_confirm(void) {
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    // set_result_perform_privacy_operation returns INT256_LENGTH (32).
    TEST_ASSERT_EQUAL(tx, INT256_LENGTH);
    TEST_ASSERT_EQUAL(g_ui_pubkey_calls, 0);
    TEST_ASSERT_EQUAL(g_ui_shared_calls, 0);
    // CWE-312 scrub: tmpCtx.publicKeyContext must be zeroed after copy.
    for (size_t i = 0; i < sizeof(tmpCtx.publicKeyContext); i++) {
        TEST_ASSERT_EQUAL(((const uint8_t *) &tmpCtx.publicKeyContext)[i], 0);
    }
}

void test_public_encryption_key_confirm_calls_ui(void) {
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_ui_pubkey_calls, 1);
    TEST_ASSERT_EQUAL(g_ui_shared_calls, 0);
    // The displayed address must be filled with the "0x" prefix.
    TEST_ASSERT_EQUAL(strings.common.toAddress[0], '0');
    TEST_ASSERT_EQUAL(strings.common.toAddress[1], 'x');
}

void test_shared_secret_confirm_calls_ui(void) {
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0x01, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_NO_RESPONSE);
    TEST_ASSERT_EQUAL(g_ui_shared_calls, 1);
    TEST_ASSERT_EQUAL(g_ui_pubkey_calls, 0);
}

void test_shared_secret_uses_the_published_key_scalar(void) {
    // The exchange must run on decode_scalar() of the derived key -- the same scalar
    // P2_PUBLIC_ENCRYPTION_KEY publishes -- not on the raw derived bytes. The derive
    // stub yields 0x11 repeated, so decode_scalar() yields 0x51, 0x11 x30, 0x10.
    uint8_t expected[32];
    memset(expected, 0x11, sizeof(expected));
    expected[0] = 0x51;
    expected[31] = 0x10;

    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0x01, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(SWO_NO_RESPONSE, sw);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, g_x25519_scalar, sizeof(expected));
}

void test_shared_secret_all_zero_output_rejected(void) {
    // RFC 7748 6.1: an all-zero X25519 result means the peer u-coordinate is a point
    // of small order and MUST be refused rather than returned as a secret.
    g_x25519_zero_output = true;

    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0x01, apdu, (uint8_t) len, &tx);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, sw);
    TEST_ASSERT_EQUAL(0, g_ui_shared_calls);
    TEST_ASSERT_EQUAL(APP_STATE_IDLE, appState);
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
    RUN_TEST(test_invalid_p1_rejected);
    RUN_TEST(test_invalid_p2_rejected);
    RUN_TEST(test_shared_secret_requires_confirmation);
    RUN_TEST(test_bad_bip32_rejected);
    RUN_TEST(test_shared_secret_short_peer_key_rejected);
    RUN_TEST(test_init_private_key_failure_propagates);
    RUN_TEST(test_public_encryption_key_non_confirm);
    RUN_TEST(test_public_encryption_key_confirm_calls_ui);
    RUN_TEST(test_shared_secret_confirm_calls_ui);
    RUN_TEST(test_shared_secret_uses_the_published_key_scalar);
    RUN_TEST(test_shared_secret_all_zero_output_rejected);
    return UNITY_END();
}
