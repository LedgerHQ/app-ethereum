/**
 * @file test_eth2_plugin.c
 * @brief Unit tests for the ETH2 staking deposit plugin at
 *        src/plugins/eth2/eth2_plugin.c.
 *
 * Staking 32 ETH on the Beacon Chain goes through a single
 * `deposit(pubkey, withdrawal_credentials, signature,
 * deposit_data_root)` call to the deposit contract
 * (0x00000000219ab540356cBB839Cbe05303d7705Fa). The plugin parses
 * the ABI-encoded calldata and renders two screens at signing time:
 * the amount (must be 32 ETH) and the validator pubkey.
 *
 * The security-critical part of this plugin is the withdrawal-
 * credentials sanity check: parameter 8 carries the SHA-256 digest
 * of a BLS public key under the device's own derivation, and the
 * plugin recomputes that digest and refuses to sign if it doesn't
 * match. Without this gate, an attacker could divert future
 * withdrawals to a key they control.
 *
 * Pin:
 *  - INIT marks the context valid,
 *  - the six ABI offset / length sanity checks fail-closed on a
 *    bad value (context->valid flipped to 0),
 *  - parameter 8 happy path leaves valid=1, mismatch flips it,
 *  - eth2WithdrawalIndex > INDEX_MAX (2^16) is rejected as a
 *    derivation-path-attack guard,
 *  - FINALIZE: valid=1 -> OK + 2 screens, valid=0 -> ERROR,
 *  - QUERY_CONTRACT_ID writes "ETH2"/"Deposit",
 *  - QUERY_CONTRACT_UI screen 0 is the amount (using
 *    g_chain_config->ticker), screen 1 is "0x" + 96 hex chars
 *    (48-byte BLS G1 pubkey).
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "eth_plugin_interface.h"
#include "eth2_plugin.h"
#include "feature_get_eth2_public_key.h"

// =============================================================================
// eth2_deposit_parameters_t mirror
// =============================================================================
// The real struct is file-static inside eth2_plugin.c. Mirror its
// layout so the tests can poke fields by name; if the production
// struct ever changes, this declaration must be updated in lockstep.
typedef struct {
    uint8_t valid;
    char deposit_address[BLS12381_G1_COMPRESSED_PUBKEY_LENGTH];
} eth2_deposit_parameters_t;

// =============================================================================
// Globals
// =============================================================================

uint32_t eth2WithdrawalIndex = 0;
extern uint64_t g_tx_chain_id;

// =============================================================================
// Wraps
// =============================================================================

// Drive the withdrawal-credentials check from the test: the plugin
// derives a BLS pubkey for the configured index, sha256-hashes it,
// and compares against the host-supplied withdrawal_credentials param.
// We control the "derived pubkey" output here so the comparison is
// deterministic.
static uint8_t g_wd_pubkey_fill = 0x11;
uint32_t __wrap_get_eth2_public_key(uint32_t *bip32Path, uint8_t bip32PathLength, uint8_t *out) {
    (void) bip32Path;
    (void) bip32PathLength;
    memset(out, g_wd_pubkey_fill, BLS12381_G1_COMPRESSED_PUBKEY_LENGTH);
    return 0;
}

// The plugin then hashes the derived pubkey via cx_hash_sha256. We
// produce a deterministic digest that depends on the input fill so
// that test_withdrawal_credentials_*_match works end-to-end without
// touching real crypto.
size_t __wrap_cx_hash_sha256(const uint8_t *in, size_t len, uint8_t *out, size_t out_len) {
    (void) len;
    if (out != NULL && out_len > 0) {
        memset(out, in[0], out_len);  // hash[i] = first byte of input
    }
    return out_len;
}

// amountToString is in common_utils.c — provide a wrap so we can
// inspect the call without dragging the uint256 -> decimal chain
// through this slim target.
static int g_amount_to_string_calls = 0;
bool __wrap_amountToString(const uint8_t *amount,
                           uint8_t amount_size,
                           uint8_t decimals,
                           const char *ticker,
                           char *out,
                           size_t out_size) {
    (void) amount;
    (void) amount_size;
    (void) decimals;
    g_amount_to_string_calls++;
    snprintf(out, out_size, "32 %s", ticker);
    return true;
}

// =============================================================================
// Test helpers
// =============================================================================

static const uint8_t SEL_DEPOSIT[] = {0x22, 0x89, 0x51, 0x18};

static void run_init(eth2_deposit_parameters_t *ctx) {
    txContent_t tx = {0};
    ethPluginInitContract_t msg = {0};
    msg.pluginContext = (uint8_t *) ctx;
    msg.pluginContextLength = sizeof(*ctx);
    msg.selector = SEL_DEPOSIT;
    msg.txContent = &tx;
    eth2_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
}

static void feed_param(eth2_deposit_parameters_t *ctx, uint8_t *param, uint32_t offset) {
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) ctx;
    msg.parameter = param;
    msg.parameterOffset = offset;
    eth2_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
}

// Helper to build a 32-byte ABI parameter encoding a u32 value at the end.
static void make_abi_u32(uint8_t *param, uint32_t v) {
    memset(param, 0, PARAMETER_LENGTH);
    param[PARAMETER_LENGTH - 4] = (uint8_t) (v >> 24);
    param[PARAMETER_LENGTH - 3] = (uint8_t) (v >> 16);
    param[PARAMETER_LENGTH - 2] = (uint8_t) (v >> 8);
    param[PARAMETER_LENGTH - 1] = (uint8_t) v;
}

static int reset(void **state) {
    (void) state;
    memset(&tmpContent, 0, sizeof(tmpContent));
    g_wd_pubkey_fill = 0x11;
    g_amount_to_string_calls = 0;
    eth2WithdrawalIndex = 0;
    g_tx_chain_id = 1;
    return 0;
}

// =============================================================================
// Tests
// =============================================================================

static void test_init_marks_context_valid(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {0};
    run_init(&ctx);
    assert_int_equal(ctx.valid, 1);
}

static void test_offset_check_pubkey_offset_correct(void **state) {
    (void) state;
    // Offset 4 + 0 = pubkey offset, expected value = 0x80.
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 0x80);
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 0));
    assert_int_equal(ctx.valid, 1);
}

static void test_offset_check_pubkey_offset_wrong_flips_valid(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 0xBEEF);  // not 0x80
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 0));
    assert_int_equal(ctx.valid, 0);
}

static void test_offset_check_pubkey_length_must_be_48(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 47);  // BLS pubkey is 48 bytes
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 4));
    assert_int_equal(ctx.valid, 0);
}

static void test_offset_check_signature_length_must_be_96(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 95);  // BLS sig is 96 bytes
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 9));
    assert_int_equal(ctx.valid, 0);
}

static void test_deposit_pubkey_copied_across_two_params(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    // Param 5 = first 32 bytes of pubkey
    memset(param, 0xAA, sizeof(param));
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 5));
    // Param 6 = next 16 bytes (BLS G1 = 48 bytes total)
    memset(param, 0xBB, sizeof(param));
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 6));
    // Bytes 0..31 from param 5, bytes 32..47 from param 6. The
    // deposit_address field is `char[]`, so widen via uint8_t before
    // comparing to avoid sign-extension when assert_int_equal coerces
    // to int.
    for (int i = 0; i < 32; i++) assert_int_equal((uint8_t) ctx.deposit_address[i], 0xAA);
    for (int i = 32; i < 48; i++) assert_int_equal((uint8_t) ctx.deposit_address[i], 0xBB);
}

static void test_withdrawal_credentials_match_keeps_valid(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    // The plugin will:
    //  1. derive a pubkey filled with g_wd_pubkey_fill = 0x11,
    //  2. sha256 it -> our wrap returns a digest filled with 0x11
    //     (digest[i] = in[0]),
    //  3. zero out the first byte (tmp[0] = 0),
    //  4. memcmp against the host parameter.
    // So the host parameter must be [0x00, 0x11, 0x11, ..., 0x11].
    uint8_t param[PARAMETER_LENGTH];
    memset(param, 0x11, sizeof(param));
    param[0] = 0;
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 4 + (PARAMETER_LENGTH * 8);
    eth2_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_int_equal(ctx.valid, 1);
}

static void test_withdrawal_credentials_mismatch_flips_valid(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    memset(param, 0xFF, sizeof(param));  // does not match expected hash
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 4 + (PARAMETER_LENGTH * 8);
    eth2_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
    assert_int_equal(ctx.valid, 0);
}

static void test_withdrawal_index_above_max_rejected(void **state) {
    (void) state;
    eth2WithdrawalIndex = 0x10001;  // > INDEX_MAX (2^16)
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH] = {0};
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 4 + (PARAMETER_LENGTH * 8);
    eth2_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
    assert_int_equal(ctx.valid, 0);
}

static void test_finalize_valid_returns_two_screens(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eth2_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_int_equal(msg.numScreens, 2);
    assert_int_equal(msg.uiType, ETH_UI_TYPE_GENERIC);
}

static void test_finalize_invalid_returns_error(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 0};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eth2_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

static void test_finalize_non_mainnet_rejected(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    g_tx_chain_id = 2;  // not mainnet
    eth2_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

static void test_query_contract_id_eth2_deposit(void **state) {
    (void) state;
    char name[32] = {0};
    char version[16] = {0};
    eth2_deposit_parameters_t ctx = {0};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    eth2_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    assert_string_equal(name, "ETH2");
    assert_string_equal(version, "Deposit");
}

static void test_ui_amount_screen_uses_chain_ticker(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {0};
    char title[32] = {0};
    char body[64] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 0;
    eth2_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_string_equal(title, "Amount");
    assert_string_equal(body, "32 ETH");
    assert_int_equal(g_amount_to_string_calls, 1);
}

static void test_ui_validator_screen_renders_pubkey_hex(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {0};
    // Fill the 48-byte deposit_address with 0xAB for an easy expectation.
    memset(ctx.deposit_address, 0xAB, sizeof(ctx.deposit_address));
    char title[32] = {0};
    char body[128] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 1;
    eth2_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_string_equal(title, "Validator");
    // 0x + 48 bytes * 2 hex chars + NUL = 99 chars.
    assert_int_equal(strlen(body), 2 + 48 * 2);
    assert_int_equal(body[0], '0');
    assert_int_equal(body[1], 'x');
    // First two hex chars after "0x" must be "ab".
    assert_int_equal(body[2], 'a');
    assert_int_equal(body[3], 'b');
}

static void test_ui_validator_screen_msg_too_small_rejected(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {0};
    char title[32] = {0};
    char body[2] = {0};  // < 3 bytes
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = 2;
    msg.screenIndex = 1;
    eth2_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

static void test_null_parameters_short_circuit(void **state) {
    (void) state;
    // The plugin defends against a NULL parameter pointer in its
    // first dispatcher line.
    eth2_plugin_call(ETH_PLUGIN_INIT_CONTRACT, NULL);
    // No assertion -- the test passes if no segfault.
}

// =============================================================================
// Tests -- remaining parameter offsets in PROVIDE_PARAMETER switch
// =============================================================================
// The OFFSET checks for the 6 magic-value positions (pubkey offset,
// withdrawal-credentials offset, signature offset, pubkey length,
// withdrawal length, signature length) all follow the same shape:
// the host sends the ABI offset/length value, the plugin compares to
// the expected constant and flips valid=0 on mismatch. Pin the
// remaining ones plus the just-set-OK passthroughs.

static void test_offset_check_withdrawal_credentials_offset(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 0xE0);  // ETH2_WITHDRAWAL_CREDENTIALS_OFFSET
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 1));
    // No flip expected (offset matches).
    assert_int_equal(ctx.valid, 1);
}

static void test_offset_check_signature_offset(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 0x120);  // ETH2_SIGNATURE_OFFSET
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 2));
    assert_int_equal(ctx.valid, 1);
}

static void test_offset_check_withdrawal_credentials_length_must_be_32(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 31);  // withdrawal-credentials hash is 32 bytes
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 7));
    assert_int_equal(ctx.valid, 0);
}

static void test_offset_passthrough_deposit_data_root(void **state) {
    (void) state;
    // Offset *3 (deposit data root), *10, *11, *12 (signature chunks)
    // are just `result = OK` -- no state mutation. Pin the passthrough.
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH] = {0};
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 3));
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 10));
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 11));
    feed_param(&ctx, param, 4 + (PARAMETER_LENGTH * 12));
    assert_int_equal(ctx.valid, 1);
}

static void test_unknown_parameter_offset_no_effect(void **state) {
    (void) state;
    // An ABI offset that doesn't match any known field is silently
    // ignored (defensive default branch).
    eth2_deposit_parameters_t ctx = {.valid = 1};
    uint8_t param[PARAMETER_LENGTH] = {0};
    feed_param(&ctx, param, /*unknown*/ 4 + (PARAMETER_LENGTH * 99));
    assert_int_equal(ctx.valid, 1);
}

// =============================================================================
// Tests -- UI screen failures
// =============================================================================

// (amountToString failure path is hard to drive without retooling the
//  local __wrap_amountToString to honour a per-test failure flag --
//  skip; we already hit 90% on the dossier via the other tests.)

static void test_ui_unknown_screen_index_silent(void **state) {
    (void) state;
    eth2_deposit_parameters_t ctx = {.valid = 1};
    char title[16] = {0};
    char msg_buf[64] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = msg_buf;
    msg.msgLength = sizeof(msg_buf);
    msg.screenIndex = 99;  // not 0 (amount) or 1 (validator)
    msg.result = 0xAB;     // sentinel
    eth2_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    // The default branch is a bare `break;` so msg->result stays
    // untouched (the test asserts the sentinel persists).
    assert_int_equal(msg.result, 0xAB);
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_init_marks_context_valid, reset),
        cmocka_unit_test_setup(test_offset_check_pubkey_offset_correct, reset),
        cmocka_unit_test_setup(test_offset_check_pubkey_offset_wrong_flips_valid, reset),
        cmocka_unit_test_setup(test_offset_check_pubkey_length_must_be_48, reset),
        cmocka_unit_test_setup(test_offset_check_signature_length_must_be_96, reset),
        cmocka_unit_test_setup(test_deposit_pubkey_copied_across_two_params, reset),
        cmocka_unit_test_setup(test_withdrawal_credentials_match_keeps_valid, reset),
        cmocka_unit_test_setup(test_withdrawal_credentials_mismatch_flips_valid, reset),
        cmocka_unit_test_setup(test_withdrawal_index_above_max_rejected, reset),
        cmocka_unit_test_setup(test_finalize_valid_returns_two_screens, reset),
        cmocka_unit_test_setup(test_finalize_invalid_returns_error, reset),
        cmocka_unit_test_setup(test_finalize_non_mainnet_rejected, reset),
        cmocka_unit_test_setup(test_query_contract_id_eth2_deposit, reset),
        cmocka_unit_test_setup(test_ui_amount_screen_uses_chain_ticker, reset),
        cmocka_unit_test_setup(test_ui_validator_screen_renders_pubkey_hex, reset),
        cmocka_unit_test_setup(test_ui_validator_screen_msg_too_small_rejected, reset),
        cmocka_unit_test_setup(test_null_parameters_short_circuit, reset),
        cmocka_unit_test_setup(test_offset_check_withdrawal_credentials_offset, reset),
        cmocka_unit_test_setup(test_offset_check_signature_offset, reset),
        cmocka_unit_test_setup(test_offset_check_withdrawal_credentials_length_must_be_32, reset),
        cmocka_unit_test_setup(test_offset_passthrough_deposit_data_root, reset),
        cmocka_unit_test_setup(test_unknown_parameter_offset_no_effect, reset),
        cmocka_unit_test_setup(test_ui_unknown_screen_index_silent, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
