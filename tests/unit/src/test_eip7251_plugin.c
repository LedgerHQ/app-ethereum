/**
 * @file test_eip7251_plugin.c
 * @brief Unit tests for the EIP-7251 validator-consolidation plugin
 *        at src/plugins/eip7251/eip7251_plugin.c.
 *
 * EIP-7251 lets a validator move its effective balance to another
 * validator (or itself, "compound") by calling the predeploy at
 * 0x0000BBdDc7CE488642fb579F8B00f3a590007251 with a 96-byte payload
 * of source_pubkey || target_pubkey. The plugin concatenates the
 * calldata into a 96-byte union and renders 1-3 screens depending
 * on whether the target differs from the source and whether a
 * native value > 1 gwei is attached.
 *
 * Same security gates as EIP-7002: has_tx_value caps the hidden
 * value budget at 1 gwei, and FINALIZE refuses a short payload.
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
#include "eip7251_plugin.h"

// =============================================================================
// eip7251_context_t mirror (file-static in the source)
// =============================================================================

#define VALIDATOR_PUBKEY_SIZE      48
#define CONSOLIDATION_REQUEST_SIZE (VALIDATOR_PUBKEY_SIZE * 2)

typedef struct {
    union {
        uint8_t consolidation_request[CONSOLIDATION_REQUEST_SIZE];
        struct {
            uint8_t source_pubkey[VALIDATOR_PUBKEY_SIZE];
            uint8_t target_pubkey[VALIDATOR_PUBKEY_SIZE];
        };
    };
    uint8_t received;
} eip7251_context_t;

// =============================================================================
// Globals
// =============================================================================

extern uint64_t g_tx_chain_id;

bool __wrap_amountToString(const uint8_t *amount,
                           uint8_t amount_size,
                           uint8_t decimals,
                           const char *ticker,
                           char *out,
                           size_t out_size) {
    (void) amount;
    (void) amount_size;
    (void) decimals;
    snprintf(out, out_size, "VAL %s", ticker);
    return true;
}

// =============================================================================
// Helpers
// =============================================================================

static void run_init(eip7251_context_t *ctx, const uint8_t *selector) {
    txContent_t tx = {0};
    ethPluginInitContract_t msg = {0};
    msg.pluginContext = (uint8_t *) ctx;
    msg.pluginContextLength = sizeof(*ctx);
    msg.selector = selector;
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
}

// =============================================================================
// Tests
// =============================================================================

static void test_init_copies_selector(void **state) {
    (void) state;
    eip7251_context_t ctx;
    memset(&ctx, 0xCC, sizeof(ctx));
    static const uint8_t selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
    run_init(&ctx, selector);
    assert_int_equal(ctx.received, CALLDATA_SELECTOR_SIZE);
    assert_int_equal(ctx.consolidation_request[0], 0xDE);
}

static void test_parameter_overflow_rejected(void **state) {
    (void) state;
    eip7251_context_t ctx = {.received = CONSOLIDATION_REQUEST_SIZE - 1};
    uint8_t chunk[32];
    memset(chunk, 0xFF, sizeof(chunk));
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = chunk;
    msg.parameter_size = 32;
    eip7251_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Branch-coverage quick wins -- NULL guards, switch defaults, edge buffers
// =============================================================================

static void test_dispatcher_null_param_silent(void **state) {
    (void) state;
    eip7251_plugin_call(ETH_PLUGIN_INIT_CONTRACT, NULL);
}

static void test_dispatcher_unknown_message_silent(void **state) {
    (void) state;
    ethPluginInitContract_t msg = {0};
    msg.result = 0xAB;
    eip7251_plugin_call((eth_plugin_msg_t) 0x7F, &msg);
    assert_int_equal(msg.result, 0xAB);
}

static void test_ui_msg_too_small_returns(void **state) {
    (void) state;
    // msgLength < 2 short-circuits BEFORE the switch.
    eip7251_context_t ctx = {0};
    char title[16] = {0};
    char msg_buf[2] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = msg_buf;
    msg.msgLength = 1;
    msg.screenIndex = 0;
    msg.result = ETH_PLUGIN_RESULT_OK;
    txContent_t tx = {0};
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_string_equal(title, "");
}

static void test_ui_unknown_screen_returns_no_result_set(void **state) {
    (void) state;
    // screenIndex not in {0, target?1, tx_value?...} -> S_UNKNOWN ->
    // bare `return` -> param->result NOT bumped.
    eip7251_context_t ctx = {0};
    char title[16] = {0};
    char msg_buf[64] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = msg_buf;
    msg.msgLength = sizeof(msg_buf);
    msg.screenIndex = 99;
    msg.result = 0xAB;
    txContent_t tx = {0};
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_int_equal(msg.result, 0xAB);
}

static void test_has_tx_value_length_above_uint64_returns_true(void **state) {
    (void) state;
    // The `value.length > sizeof(uint64_t)` early-true branch in
    // has_tx_value never fires on the existing tests (all use
    // length<=8). FINALIZE with a >8-byte value reaches it and the
    // tx-value extra screen gets added.
    eip7251_context_t ctx = {.received = CONSOLIDATION_REQUEST_SIZE};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xBB, VALIDATOR_PUBKEY_SIZE);  // target != source
    txContent_t tx = {0};
    tx.value.length = 9;  // > sizeof(uint64_t)
    memset(tx.value.value, 0xFF, tx.value.length);
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    // 2 screens for consolidate (source + target) + 1 for tx value = 3.
    assert_int_equal(msg.numScreens, 3);
}

static void test_parameter_success_copies_into_context(void **state) {
    (void) state;
    // Normal path: ctx empty, push a 32-byte chunk -> memcpy into
    // consolidation_request, received bumps to 32, result OK.
    eip7251_context_t ctx = {0};
    uint8_t chunk[32];
    memset(chunk, 0x42, sizeof(chunk));
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = chunk;
    msg.parameter_size = sizeof(chunk);
    eip7251_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_int_equal(ctx.received, 32);
    assert_int_equal(ctx.consolidation_request[0], 0x42);
    assert_int_equal(ctx.consolidation_request[31], 0x42);
}

static void test_finalize_non_mainnet_rejected(void **state) {
    (void) state;
    eip7251_context_t ctx = {.received = CONSOLIDATION_REQUEST_SIZE};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    g_tx_chain_id = 2;  // not mainnet
    eip7251_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    g_tx_chain_id = 1;  // restore
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

static void test_finalize_incomplete_rejected(void **state) {
    (void) state;
    eip7251_context_t ctx = {.received = CONSOLIDATION_REQUEST_SIZE - 1};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

static void test_finalize_compound_single_screen(void **state) {
    (void) state;
    // target == source → "compound" → numScreens = 1.
    eip7251_context_t ctx = {.received = CONSOLIDATION_REQUEST_SIZE};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.result, ETH_PLUGIN_RESULT_OK);
    assert_int_equal(msg.numScreens, 1);
}

static void test_finalize_consolidate_two_screens(void **state) {
    (void) state;
    eip7251_context_t ctx = {.received = CONSOLIDATION_REQUEST_SIZE};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xBB, VALIDATOR_PUBKEY_SIZE);
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.numScreens, 2);
}

static void test_finalize_with_tx_value_extra_screen(void **state) {
    (void) state;
    eip7251_context_t ctx = {.received = CONSOLIDATION_REQUEST_SIZE};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);  // compound
    txContent_t tx = {0};
    tx.value.value[0] = 0x3B;
    tx.value.value[1] = 0x9A;
    tx.value.value[2] = 0xCA;
    tx.value.value[3] = 0x01;  // 1 gwei + 1
    tx.value.length = 4;
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    eip7251_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    assert_int_equal(msg.numScreens, 2);  // compound + tx_value
}

static void test_query_contract_id_compound_vs_consolidate(void **state) {
    (void) state;
    char name[16] = {0};
    char version[16] = {0};
    ethQueryContractID_t msg = {0};
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);

    eip7251_context_t ctx = {0};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    msg.pluginContext = (uint8_t *) &ctx;
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    assert_string_equal(name, "stake");
    assert_string_equal(version, "compound");

    memset(ctx.target_pubkey, 0xBB, VALIDATOR_PUBKEY_SIZE);
    memset(version, 0, sizeof(version));
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    assert_string_equal(version, "consolidate");
}

static void test_ui_compound_uses_single_validator_title(void **state) {
    (void) state;
    eip7251_context_t ctx = {0};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);  // compound
    txContent_t tx = {0};
    char title[32], body[128];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 0;
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_string_equal(title, "Validator");
}

static void test_ui_consolidate_uses_from_to_titles(void **state) {
    (void) state;
    eip7251_context_t ctx = {0};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xBB, VALIDATOR_PUBKEY_SIZE);  // consolidate
    txContent_t tx = {0};
    char title[32], body[128];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 0;
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_string_equal(title, "From validator");

    msg.screenIndex = 1;
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_string_equal(title, "To validator");
}

static void test_ui_tx_value_screen_dynamic_index(void **state) {
    (void) state;
    eip7251_context_t ctx = {0};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);  // compound → 1 base screen
    txContent_t tx = {0};
    tx.value.value[0] = 0x3B;
    tx.value.value[1] = 0x9A;
    tx.value.value[2] = 0xCA;
    tx.value.value[3] = 0x01;
    tx.value.length = 4;
    char title[32], body[64];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 1;  // tx_value sits at idx 1 in compound mode
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    assert_string_equal(title, "Tx value");
    assert_string_equal(body, "VAL ETH");
}

static void test_ui_msg_too_small_short_circuits(void **state) {
    (void) state;
    eip7251_context_t ctx = {0};
    memset(ctx.source_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    memset(ctx.target_pubkey, 0xAA, VALIDATOR_PUBKEY_SIZE);
    txContent_t tx = {0};
    char title[32] = "untouched";
    char body[1] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = 1;
    msg.screenIndex = 0;
    eip7251_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    // The title is not touched when msgLength < 2.
    assert_string_equal(title, "untouched");
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_copies_selector),
        cmocka_unit_test(test_parameter_overflow_rejected),
        cmocka_unit_test(test_parameter_success_copies_into_context),
        cmocka_unit_test(test_dispatcher_null_param_silent),
        cmocka_unit_test(test_dispatcher_unknown_message_silent),
        cmocka_unit_test(test_ui_msg_too_small_returns),
        cmocka_unit_test(test_ui_unknown_screen_returns_no_result_set),
        cmocka_unit_test(test_has_tx_value_length_above_uint64_returns_true),
        cmocka_unit_test(test_finalize_non_mainnet_rejected),
        cmocka_unit_test(test_finalize_incomplete_rejected),
        cmocka_unit_test(test_finalize_compound_single_screen),
        cmocka_unit_test(test_finalize_consolidate_two_screens),
        cmocka_unit_test(test_finalize_with_tx_value_extra_screen),
        cmocka_unit_test(test_query_contract_id_compound_vs_consolidate),
        cmocka_unit_test(test_ui_compound_uses_single_validator_title),
        cmocka_unit_test(test_ui_consolidate_uses_from_to_titles),
        cmocka_unit_test(test_ui_tx_value_screen_dynamic_index),
        cmocka_unit_test(test_ui_msg_too_small_short_circuits),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
