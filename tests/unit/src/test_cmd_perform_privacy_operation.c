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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_perform_privacy_operation.h"
#include "wraps.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// parseBip32 is wrapped in mocks/mock.c; toggle g_parsebip32_force_null
// from wraps.h to drive the negative tests.

// os_derive_bip32_no_throw is a static inline that delegates to
// os_derive_bip32_with_seed_no_throw, which itself wraps the syscall
// os_perso_derive_node_with_seed_key inside BEGIN_TRY/TRY. The
// syscall is the one we can wrap at link time.
static int g_derive_should_throw = 0;
void __wrap_os_perso_derive_node_with_seed_key(unsigned int mode,
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
cx_err_t __wrap_cx_ecfp_init_private_key_no_throw(cx_curve_t curve,
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
cx_err_t __wrap_cx_ecfp_generate_pair_no_throw(cx_curve_t curve,
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
cx_err_t __wrap_cx_x25519(uint8_t *p, const uint8_t *s, size_t s_len) {
    (void) s;
    (void) s_len;
    if (p != NULL) {
        // Overwrite the host-supplied peer key with deterministic
        // shared-secret bytes so the response buffer can be inspected.
        memset(p, 0x99, 32);
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
    assert_true(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static int reset(void **state) {
    (void) state;
    appState = APP_STATE_IDLE;
    g_parsebip32_force_null = false;
    g_init_priv_ret = CX_OK;
    g_gen_pair_ret = CX_OK;
    g_x25519_ret = CX_OK;
    g_ui_pubkey_calls = 0;
    g_ui_shared_calls = 0;
    memset(&tmpCtx, 0, sizeof(tmpCtx));
    memset(&strings, 0, sizeof(strings));
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
    return 0;
}

// =============================================================================
// Tests — dispatcher guards
// =============================================================================

static void test_invalid_p1_rejected(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(/*p1=*/0x05, 0, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_WRONG_P1_P2);
}

static void test_invalid_p2_rejected(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw =
        handle_perform_privacy_operation(P1_NON_CONFIRM, /*p2=*/0x05, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_WRONG_P1_P2);
}

static void test_shared_secret_requires_confirmation(void **state) {
    (void) state;
    // CWE-200 guard: shared-secret export must require P1_CONFIRM.
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM,
                                                   /*p2=*/0x01,  // P2_SHARED_SECRET
                                                   apdu,
                                                   (uint8_t) len,
                                                   &tx);
    assert_int_equal(sw, SWO_CONDITIONS_NOT_SATISFIED);
    // No UI / no key material released.
    assert_int_equal(g_ui_pubkey_calls, 0);
    assert_int_equal(g_ui_shared_calls, 0);
}

static void test_bad_bip32_rejected(void **state) {
    (void) state;
    g_parsebip32_force_null = true;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_shared_secret_short_peer_key_rejected(void **state) {
    (void) state;
    // P2=SHARED_SECRET requires the peer public key (32 bytes) right
    // after the BIP-32 path.
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);  // no 32-byte key
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0x01, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_WRONG_DATA_LENGTH);
}

// =============================================================================
// Tests — derivation / SDK failures propagate
// =============================================================================

static void test_init_private_key_failure_propagates(void **state) {
    (void) state;
    g_init_priv_ret = CX_INVALID_PARAMETER;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, (uint16_t) CX_INVALID_PARAMETER);
}

// =============================================================================
// Tests — happy paths
// =============================================================================

static void test_public_encryption_key_non_confirm(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_NON_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_SUCCESS);
    // set_result_perform_privacy_operation returns INT256_LENGTH (32).
    assert_int_equal(tx, INT256_LENGTH);
    assert_int_equal(g_ui_pubkey_calls, 0);
    assert_int_equal(g_ui_shared_calls, 0);
    // CWE-312 scrub: tmpCtx.publicKeyContext must be zeroed after copy.
    for (size_t i = 0; i < sizeof(tmpCtx.publicKeyContext); i++) {
        assert_int_equal(((const uint8_t *) &tmpCtx.publicKeyContext)[i], 0);
    }
}

static void test_public_encryption_key_confirm_calls_ui(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_NO_RESPONSE);
    assert_int_equal(g_ui_pubkey_calls, 1);
    assert_int_equal(g_ui_shared_calls, 0);
    // The displayed address must be filled with the "0x" prefix.
    assert_int_equal(strings.common.toAddress[0], '0');
    assert_int_equal(strings.common.toAddress[1], 'x');
}

static void test_shared_secret_confirm_calls_ui(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true);
    unsigned int tx = 0;
    uint16_t sw = handle_perform_privacy_operation(P1_CONFIRM, 0x01, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_NO_RESPONSE);
    assert_int_equal(g_ui_shared_calls, 1);
    assert_int_equal(g_ui_pubkey_calls, 0);
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_invalid_p1_rejected, reset),
        cmocka_unit_test_setup(test_invalid_p2_rejected, reset),
        cmocka_unit_test_setup(test_shared_secret_requires_confirmation, reset),
        cmocka_unit_test_setup(test_bad_bip32_rejected, reset),
        cmocka_unit_test_setup(test_shared_secret_short_peer_key_rejected, reset),
        cmocka_unit_test_setup(test_init_private_key_failure_propagates, reset),
        cmocka_unit_test_setup(test_public_encryption_key_non_confirm, reset),
        cmocka_unit_test_setup(test_public_encryption_key_confirm_calls_ui, reset),
        cmocka_unit_test_setup(test_shared_secret_confirm_calls_ui, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
