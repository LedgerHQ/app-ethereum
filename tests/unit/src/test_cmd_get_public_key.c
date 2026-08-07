/**
 * @file test_cmd_get_public_key.c
 * @brief Unit tests for the GET_PUBLIC_KEY handler at
 *        src/features/get_public_key/cmd_get_public_key.c
 *        and the underlying derivation helpers in
 *        src/features/get_public_key/get_public_key.c.
 *
 * GET_PUBLIC_KEY is what wallets call to learn the address that
 * corresponds to a given BIP-32 path on this device. The handler:
 *   - parses (and validates) the requested BIP-32 path,
 *   - derives the SECP256K1 public key on that path,
 *   - turns it into an Ethereum address (keccak-256(pubkey)[12..32]),
 *   - optionally pins the chain_id (so clones don't accept paths
 *     intended for mainnet),
 *   - either reply immediately (P1_NON_CONFIRM) or show the address
 *     on screen for user confirmation (P1_CONFIRM).
 *
 * A bug in this path is high-impact:
 *   - the wrong address can be shown to the user (they think they
 *     own X but really own Y → lost funds when they share the addr),
 *   - the response layout can let an attacker smuggle extra bytes
 *     past the address (host-side parser confusion),
 *   - the chain-id check can be bypassed (clone app handing out
 *     a mainnet address while pretending to be a fork).
 *
 * Pin every P1/P2 combination, the parse / chain / leftover gates,
 * the failure paths, and the wire-format of set_result_get_publicKey.
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
#include "get_public_key.h"
#include "common_utils.h"
#include "wraps.h"

// =============================================================================
// Globals required by linked translation units
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// parseBip32 is wrapped in mocks/mock.c; toggle g_parsebip32_force_null
// from wraps.h to drive the negative tests.

// bip32_derive_get_pubkey_256 is a `static inline` in
// crypto_helpers.h that delegates to bip32_derive_with_seed_get_pubkey_256;
// wrap the underlying primitive so we control its outcome.
static cx_err_t g_derive_ret = CX_OK;
cx_err_t __wrap_bip32_derive_with_seed_get_pubkey_256(unsigned int derivation_mode,
                                                      cx_curve_t curve,
                                                      const uint32_t *path,
                                                      size_t path_len,
                                                      uint8_t raw_pubkey[static 65],
                                                      uint8_t *chain_code,
                                                      cx_md_t hashID,
                                                      unsigned char *seed,
                                                      size_t seed_len) {
    (void) derivation_mode;
    (void) curve;
    (void) path;
    (void) path_len;
    (void) hashID;
    (void) seed;
    (void) seed_len;
    memset(raw_pubkey, 0x55, 65);
    if (chain_code != NULL) {
        memset(chain_code, 0xCC, 32);
    }
    return g_derive_ret;
}

static int g_eth_addr_from_raw_calls = 0;
void __wrap_getEthAddressFromRawKey(const uint8_t *publicKey, uint8_t *out) {
    (void) publicKey;
    g_eth_addr_from_raw_calls++;
    memset(out, 0xAA, 20);
}

static int g_eth_addr_string_calls = 0;
void __wrap_getEthAddressStringFromRawKey(const uint8_t *publicKey, char *out, uint64_t chain_id) {
    (void) publicKey;
    (void) chain_id;
    g_eth_addr_string_calls++;
    // ADDRESS_LENGTH_HEX = 40, then NUL = 41
    memset(out, 'A', 40);
    out[40] = '\0';
}

static int g_reset_calls = 0;
void __wrap_reset_app_context(void) {
    g_reset_calls++;
}

static int g_ui_display_calls = 0;
static uint64_t g_ui_display_chain;
void ui_display_public_key(const uint64_t *chain_id) {
    g_ui_display_calls++;
    if (chain_id != NULL) g_ui_display_chain = *chain_id;
}

// =============================================================================
// APDU builder
// =============================================================================

static size_t build_apdu(uint8_t *out, size_t out_size, bool include_chain_id, uint64_t chain_id) {
    size_t off = 0;
    out[off++] = 5;  // bip32 path length
    for (int i = 0; i < 5; i++) {
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
        out[off++] = 0;
    }
    if (include_chain_id) {
        for (int i = 7; i >= 0; i--) {
            out[off++] = (uint8_t) (chain_id >> (8 * i));
        }
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
    G_called_from_swap = false;
    g_parsebip32_force_null = false;
    g_derive_ret = CX_OK;
    g_eth_addr_from_raw_calls = 0;
    g_eth_addr_string_calls = 0;
    g_reset_calls = 0;
    g_ui_display_calls = 0;
    g_ui_display_chain = 0;
    g_chainConfig.chain_id = 1;
    memset(&tmpCtx, 0, sizeof(tmpCtx));
    memset(&strings, 0, sizeof(strings));
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
    return 0;
}

// =============================================================================
// Tests — handle_get_public_key
// =============================================================================

static void test_no_reset_when_idle_and_not_from_swap(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    (void) handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(g_reset_calls, 0);
}

static void test_skips_reset_when_called_from_swap(void **state) {
    (void) state;
    G_called_from_swap = true;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    (void) handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(g_reset_calls, 0);
}

static void test_invalid_p1_rejected(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(/*p1=*/0x05, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_WRONG_P1_P2);
}

static void test_invalid_p2_rejected(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, /*p2=*/0x05, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_WRONG_P1_P2);
}

static void test_bad_bip32_rejected(void **state) {
    (void) state;
    g_parsebip32_force_null = true;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_derive_failure_rejected(void **state) {
    (void) state;
    g_derive_ret = CX_INVALID_PARAMETER;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_non_confirm_happy_path_writes_response(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_SUCCESS);
    // set_result_get_publicKey layout:
    //   [pubkey_len=65] [65 bytes pubkey] [addr_len=40] [40 bytes addr]
    // Total without chaincode: 1 + 65 + 1 + 40 = 107.
    assert_int_equal(tx, 1 + 65 + 1 + 40);
    assert_int_equal(G_io_tx_buffer[0], 65);
    assert_int_equal(G_io_tx_buffer[66], 40);
    // No UI on NON_CONFIRM.
    assert_int_equal(g_ui_display_calls, 0);
}

static void test_chaincode_path_extends_response(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    (void) handle_get_public_key(P1_NON_CONFIRM, P2_CHAINCODE, apdu, (uint8_t) len, &tx);
    // P2_CHAINCODE appends 32 chain-code bytes after the address.
    assert_int_equal(tx, 1 + 65 + 1 + 40 + 32);
    // The chaincode bytes the wrap filled with 0xCC must land after
    // the address (offset 1 + 65 + 1 + 40 = 107).
    assert_int_equal(G_io_tx_buffer[107], 0xCC);
}

static void test_confirm_calls_ui_and_returns_no_response(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), false, 0);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_NO_RESPONSE);
    assert_int_equal(g_ui_display_calls, 1);
    // strings.common.toAddress must be filled with "0x" + 40 hex chars.
    assert_int_equal(strings.common.toAddress[0], '0');
    assert_int_equal(strings.common.toAddress[1], 'x');
}

static void test_chain_id_mismatch_rejected_on_clone(void **state) {
    (void) state;
    g_chainConfig.chain_id = 137;  // Polygon (clone)
    uint8_t apdu[64];
    // Host sends chain_id = 1 (ETH mainnet) — the clone must refuse.
    size_t len = build_apdu(apdu, sizeof(apdu), true, 1);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_chain_id_match_accepted_on_clone(void **state) {
    (void) state;
    g_chainConfig.chain_id = 137;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true, 137);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_SUCCESS);
}

static void test_mainnet_app_accepts_any_chain_id(void **state) {
    (void) state;
    g_chainConfig.chain_id = ETHEREUM_MAINNET_CHAINID;
    uint8_t apdu[64];
    // Host sends 137; the mainnet app does NOT enforce chain_id match.
    size_t len = build_apdu(apdu, sizeof(apdu), true, 137);
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_SUCCESS);
}

static void test_leftover_bytes_rejected(void **state) {
    (void) state;
    uint8_t apdu[64];
    size_t len = build_apdu(apdu, sizeof(apdu), true, 1);
    // Append one extra trailing byte the parser doesn't expect.
    apdu[len++] = 0xCC;
    unsigned int tx = 0;
    uint16_t sw = handle_get_public_key(P1_NON_CONFIRM, P2_NO_CHAINCODE, apdu, (uint8_t) len, &tx);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

// =============================================================================
// Tests — get_public_key (the version called from sign-tx flows)
// =============================================================================

static void test_get_public_key_short_buffer_rejected(void **state) {
    (void) state;
    uint8_t out[10];  // < ADDRESS_LENGTH
    uint16_t sw = get_public_key(out, sizeof(out));
    assert_int_equal(sw, SWO_WRONG_DATA_LENGTH);
}

static void test_get_public_key_derive_failure_propagates(void **state) {
    (void) state;
    g_derive_ret = CX_INVALID_PARAMETER;
    uint8_t out[20];
    uint16_t sw = get_public_key(out, sizeof(out));
    assert_int_equal(sw, (uint16_t) CX_INVALID_PARAMETER);
}

static void test_get_public_key_happy_path(void **state) {
    (void) state;
    uint8_t out[20];
    memset(out, 0, sizeof(out));
    uint16_t sw = get_public_key(out, sizeof(out));
    assert_int_equal(sw, SWO_SUCCESS);
    assert_int_equal(g_eth_addr_from_raw_calls, 1);
    // The wrap fills out with 0xAA.
    for (int i = 0; i < 20; i++) {
        assert_int_equal(out[i], 0xAA);
    }
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_no_reset_when_idle_and_not_from_swap, reset),
        cmocka_unit_test_setup(test_skips_reset_when_called_from_swap, reset),
        cmocka_unit_test_setup(test_invalid_p1_rejected, reset),
        cmocka_unit_test_setup(test_invalid_p2_rejected, reset),
        cmocka_unit_test_setup(test_bad_bip32_rejected, reset),
        cmocka_unit_test_setup(test_derive_failure_rejected, reset),
        cmocka_unit_test_setup(test_non_confirm_happy_path_writes_response, reset),
        cmocka_unit_test_setup(test_chaincode_path_extends_response, reset),
        cmocka_unit_test_setup(test_confirm_calls_ui_and_returns_no_response, reset),
        cmocka_unit_test_setup(test_chain_id_mismatch_rejected_on_clone, reset),
        cmocka_unit_test_setup(test_chain_id_match_accepted_on_clone, reset),
        cmocka_unit_test_setup(test_mainnet_app_accepts_any_chain_id, reset),
        cmocka_unit_test_setup(test_leftover_bytes_rejected, reset),
        cmocka_unit_test_setup(test_get_public_key_short_buffer_rejected, reset),
        cmocka_unit_test_setup(test_get_public_key_derive_failure_propagates, reset),
        cmocka_unit_test_setup(test_get_public_key_happy_path, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
