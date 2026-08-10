/**
 * @file test_eip7002_plugin.c
 * @brief Unit tests for the EIP-7002 validator-withdrawal plugin at
 *        src/plugins/eip7002/eip7002_plugin.c.
 *
 * EIP-7002 lets a validator request a withdrawal from the consensus
 * layer by calling the predeploy contract
 * 0x00000961Ef480Eb55e80D19ad83579A64c007002 with a packed payload
 * of `validator_pubkey || amount` (48 + 8 bytes). The plugin
 * concatenates the selector + parameters into a single buffer and
 * renders 1-3 screens depending on whether the tx carries a native
 * value and whether the request is full-exit (amount=0) or partial.
 *
 * The interesting security gate is has_tx_value: the protocol fee
 * is normally in the wei-to-gwei range, so showing a 1-wei screen
 * on every legitimate request is noisy. The plugin hides the tx
 * value as long as it stays below 1 gwei. That caps the attacker
 * "hidden value" budget at dust ($0.000004 at $4000/ETH). Pin both
 * sides of the threshold.
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
#include "eip7002_plugin.h"

// =============================================================================
// eip7002_context_t mirror (file-static in the source)
// =============================================================================

#define VALIDATOR_PUBKEY_SIZE   48
#define AMOUNT_SIZE             8
#define WITHDRAWAL_REQUEST_SIZE (VALIDATOR_PUBKEY_SIZE + AMOUNT_SIZE)

typedef struct {
    union {
        uint8_t withdrawal_request[WITHDRAWAL_REQUEST_SIZE];
        struct {
            uint8_t validator_pubkey[VALIDATOR_PUBKEY_SIZE];
            uint8_t raw_amount[AMOUNT_SIZE];
        };
    };
    uint8_t received;
} eip7002_context_t;

// =============================================================================
// Globals
// =============================================================================

extern uint64_t g_tx_chain_id;

// =============================================================================
// Wraps
// =============================================================================

// get_tx_chain_id is wrapped in mocks/mock.c (returns 1 by default).

// get_displayable_ticker is wrapped in mocks/mock.c (defaults to "ETH").

static int g_amount_calls = 0;
bool __wrap_amountToString(const uint8_t *amount,
                           uint8_t amount_size,
                           uint8_t decimals,
                           const char *ticker,
                           char *out,
                           size_t out_size) {
    (void) amount;
    (void) amount_size;
    (void) decimals;
    g_amount_calls++;
    snprintf(out, out_size, "FMT %s", ticker);
    return true;
}

// =============================================================================
// Test helpers
// =============================================================================

static void run_init(eip7002_context_t *ctx, const uint8_t *selector, const txContent_t *tx) {
    ethPluginInitContract_t msg = {0};
    msg.pluginContext = (uint8_t *) ctx;
    msg.pluginContextLength = sizeof(*ctx);
    msg.selector = selector;
    msg.txContent = tx;
    eip7002_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
}

static void feed_param(eip7002_context_t *ctx, const uint8_t *data, uint8_t size) {
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) ctx;
    msg.parameter = data;
    msg.parameter_size = size;
    eip7002_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
}

static int reset(void **state) {
    (void) state;
    g_amount_calls = 0;
    g_tx_chain_id = 1;
    return 0;
}

// =============================================================================
// Tests — INIT / PROVIDE_PARAMETER concatenation
// =============================================================================

static void test_init_copies_selector_and_resets_received(void **state) {
    (void) state;
    eip7002_context_t ctx;
    memset(&ctx, 0xCC, sizeof(ctx));  // dirty
    static const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    txContent_t tx = {0};
    run_init(&ctx, selector, &tx);
    assert_int_equal(ctx.received, CALLDATA_SELECTOR_SIZE);
    assert_int_equal(ctx.withdrawal_request[0], 0xDE);
    assert_int_equal(ctx.withdrawal_request[3], 0xEF);
    // The rest of the context must be zeroed (explicit_bzero in source).
    assert_int_equal(ctx.withdrawal_request[4], 0);
    assert_int_equal(ctx.withdrawal_request[55], 0);
}

static void test_parameters_concatenate_into_request(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};
    static const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    txContent_t tx = {0};
    run_init(&ctx, selector, &tx);
    // The plugin packs everything into a 56-byte withdrawal_request
    // union: selector_bytes... + parameter_bytes... up to 56 total.
    // After INIT we have received=4 (selector), so 52 bytes of params
    // are available (32 + 20).
    uint8_t chunk[32];
    memset(chunk, 0xAA, sizeof(chunk));
    feed_param(&ctx, chunk, 32);
    assert_int_equal(ctx.received, CALLDATA_SELECTOR_SIZE + 32);
    feed_param(&ctx, chunk, 20);
    assert_int_equal(ctx.received, WITHDRAWAL_REQUEST_SIZE);  // 56
    // raw_amount sits at offset 48 within the union; the last 8 bytes
    // of the 52 parameter bytes land there.
    for (int i = 0; i < 8; i++) assert_int_equal(ctx.raw_amount[i], 0xAA);
}

static void test_parameter_overflow_rejected(void **state) {
    (void) state;
    eip7002_context_t ctx = {.received = WITHDRAWAL_REQUEST_SIZE - 1};
    uint8_t chunk[32];
    memset(chunk, 0xFF, sizeof(chunk));
    // received(=55) + parameter_size(=32) = 87 > 56 → reject.
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = chunk;
    msg.parameter_size = 32;
    eip7002_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Tests — FINALIZE
// =============================================================================

static void test_finalize_complete_request_one_screen(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};
    ctx.received = WITHDRAWAL_REQUEST_SIZE;  // full exit (raw_amount=0)
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_int_equal(msg.numScreens, 1);  // Validator only
    assert_int_equal(msg.uiType, ETH_UI_TYPE_GENERIC);
}

static void test_finalize_incomplete_rejected(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};
    ctx.received = WITHDRAWAL_REQUEST_SIZE - 1;
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

static void test_finalize_non_mainnet_rejected(void **state) {
    (void) state;
    eip7002_context_t ctx = {.received = WITHDRAWAL_REQUEST_SIZE};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    g_tx_chain_id = 2;  // not mainnet
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

static void test_finalize_partial_withdrawal_extra_screen(void **state) {
    (void) state;
    eip7002_context_t ctx = {.received = WITHDRAWAL_REQUEST_SIZE};
    ctx.raw_amount[7] = 0x42;  // non-zero → partial
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.numScreens, 2);  // Validator + Amount
}

static void test_finalize_tx_value_above_threshold_extra_screen(void **state) {
    (void) state;
    // 1 gwei + 1 wei = 1_000_000_001 wei = 0x3B9ACA01. Encoded BE as 4 bytes.
    eip7002_context_t ctx = {.received = WITHDRAWAL_REQUEST_SIZE};
    txContent_t tx = {0};
    tx.value.value[0] = 0x3B;
    tx.value.value[1] = 0x9A;
    tx.value.value[2] = 0xCA;
    tx.value.value[3] = 0x01;
    tx.value.length = 4;
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.numScreens, 2);  // Validator + Tx value
}

static void test_finalize_tx_value_below_threshold_hidden(void **state) {
    (void) state;
    // 1 gwei exactly is at the threshold (`val > 1e9`), so a value of
    // EXACTLY 1e9 wei is NOT shown. Use 0x3B9ACA00 = 1_000_000_000.
    eip7002_context_t ctx = {.received = WITHDRAWAL_REQUEST_SIZE};
    txContent_t tx = {0};
    tx.value.value[0] = 0x3B;
    tx.value.value[1] = 0x9A;
    tx.value.value[2] = 0xCA;
    tx.value.value[3] = 0x00;
    tx.value.length = 4;
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.numScreens, 1);  // Tx value hidden
}

static void test_finalize_tx_value_huge_shown(void **state) {
    (void) state;
    // value.length > 8 bytes -> unambiguously > 2^64 -> shown.
    eip7002_context_t ctx = {.received = WITHDRAWAL_REQUEST_SIZE};
    txContent_t tx = {0};
    memset(tx.value.value, 0xFF, 16);
    tx.value.length = 16;
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.numScreens, 2);
}

// =============================================================================
// Tests — QUERY_CONTRACT_ID
// =============================================================================

static void test_query_contract_id_full_exit(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};  // raw_amount = 0
    char name[32] = {0};
    char version[16] = {0};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    eip7002_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    assert_string_equal(name, "full exit");
    assert_string_equal(version, "do");
}

static void test_query_contract_id_partial_withdrawal(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};
    ctx.raw_amount[7] = 0x42;
    char name[32] = {0};
    char version[16] = {0};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    eip7002_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    assert_string_equal(name, "partial withdrawal");
}

// =============================================================================
// Tests — QUERY_CONTRACT_UI
// =============================================================================

static void test_ui_validator_screen_renders_pubkey(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};
    memset(ctx.validator_pubkey, 0xAB, sizeof(ctx.validator_pubkey));
    txContent_t tx = {0};
    char title[32] = {0};
    char body[128] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 0;
    eip7002_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_string_equal(title, "Validator");
    assert_int_equal(body[0], '0');
    assert_int_equal(body[1], 'x');
    // format_hex emits UPPERCASE; "AB" expected.
    assert_int_equal(body[2], 'A');
    assert_int_equal(body[3], 'B');
}

static void test_ui_tx_value_screen_when_above_threshold(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};
    txContent_t tx = {0};
    tx.value.value[0] = 0x3B;
    tx.value.value[1] = 0x9A;
    tx.value.value[2] = 0xCA;
    tx.value.value[3] = 0x01;
    tx.value.length = 4;
    char title[32] = {0};
    char body[64] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 1;
    eip7002_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_string_equal(title, "Tx value");
    assert_string_equal(body, "FMT ETH");
}

static void test_ui_amount_screen_when_partial(void **state) {
    (void) state;
    eip7002_context_t ctx = {0};
    ctx.raw_amount[7] = 0x42;
    txContent_t tx = {0};  // tx.value = 0 → no tx_value screen
    char title[32] = {0};
    char body[64] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 1;  // Amount sits at idx 1 when tx_value hidden
    eip7002_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_string_equal(title, "Amount");
}

// =============================================================================
// Branch-coverage quick wins -- NULL guards, switch defaults, edge buffers
// =============================================================================

static void test_dispatcher_null_param_silent(void **state) {
    (void) state;
    // The dispatcher's outer `if (param != NULL)` guards every sub-handler.
    // Passing NULL must short-circuit silently rather than crash.
    eip7002_plugin_call(ETH_PLUGIN_INIT_CONTRACT, NULL);
}

static void test_dispatcher_unknown_message_silent(void **state) {
    (void) state;
    // switch (msg) default branch: an unmapped eth_plugin_msg_t value
    // hits the PRINTF default and returns without touching param.
    ethPluginInitContract_t msg = {0};
    msg.result = 0xAB;  // sentinel
    eip7002_plugin_call((eth_plugin_msg_t) 0x7F, &msg);
    assert_int_equal(msg.result, 0xAB);
}

static void test_ui_validator_msg_too_small_short_circuits(void **state) {
    (void) state;
    // msgLength < 2 means we can't even write the "0x" prefix.
    eip7002_context_t ctx = {0};
    char title[16] = {0};
    char msg_buf[4] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = msg_buf;
    msg.msgLength = 1;    // too small for "0x"
    msg.screenIndex = 0;  // S_VALIDATOR
    msg.result = ETH_PLUGIN_RESULT_OK;
    txContent_t tx = {0};
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    // The S_VALIDATOR path returns early on msgLength<2 without
    // writing "Validator" into the title.
    assert_string_equal(title, "");
}

static void test_ui_unknown_screen_index_no_op(void **state) {
    (void) state;
    // screenIndex doesn't match S_VALIDATOR / S_TX_VALUE / S_REQUEST_AMOUNT
    // -> falls into S_UNKNOWN -> bare `break` -> result set to OK at end.
    eip7002_context_t ctx = {0};
    char title[16] = {0};
    char msg_buf[64] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = msg_buf;
    msg.msgLength = sizeof(msg_buf);
    msg.screenIndex = 99;
    txContent_t tx = {0};
    msg.txContent = &tx;
    eip7002_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_string_equal(title, "");
}

static void test_has_tx_value_null_txcontent_returns_false(void **state) {
    (void) state;
    // has_tx_value is static; reach it through FINALIZE with NULL
    // txContent. has_tx_value()'s first OR clause catches NULL and
    // returns false -- the finalize then proceeds without adding the
    // tx-value extra screen.
    eip7002_context_t ctx = {.received = WITHDRAWAL_REQUEST_SIZE};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = NULL;
    eip7002_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_init_copies_selector_and_resets_received, reset),
        cmocka_unit_test_setup(test_parameters_concatenate_into_request, reset),
        cmocka_unit_test_setup(test_parameter_overflow_rejected, reset),
        cmocka_unit_test_setup(test_finalize_complete_request_one_screen, reset),
        cmocka_unit_test_setup(test_finalize_incomplete_rejected, reset),
        cmocka_unit_test_setup(test_finalize_non_mainnet_rejected, reset),
        cmocka_unit_test_setup(test_finalize_partial_withdrawal_extra_screen, reset),
        cmocka_unit_test_setup(test_finalize_tx_value_above_threshold_extra_screen, reset),
        cmocka_unit_test_setup(test_finalize_tx_value_below_threshold_hidden, reset),
        cmocka_unit_test_setup(test_finalize_tx_value_huge_shown, reset),
        cmocka_unit_test_setup(test_query_contract_id_full_exit, reset),
        cmocka_unit_test_setup(test_query_contract_id_partial_withdrawal, reset),
        cmocka_unit_test_setup(test_ui_validator_screen_renders_pubkey, reset),
        cmocka_unit_test_setup(test_ui_tx_value_screen_when_above_threshold, reset),
        cmocka_unit_test_setup(test_ui_amount_screen_when_partial, reset),
        cmocka_unit_test_setup(test_dispatcher_null_param_silent, reset),
        cmocka_unit_test_setup(test_dispatcher_unknown_message_silent, reset),
        cmocka_unit_test_setup(test_ui_validator_msg_too_small_short_circuits, reset),
        cmocka_unit_test_setup(test_ui_unknown_screen_index_no_op, reset),
        cmocka_unit_test_setup(test_has_tx_value_null_txcontent_returns_false, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
