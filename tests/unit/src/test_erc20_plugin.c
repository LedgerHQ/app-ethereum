/**
 * @file test_erc20_plugin.c
 * @brief Unit tests for the ERC-20 plugin at
 *        src/plugins/erc20/erc20_plugin.c.
 *
 * The plugin dispatches on eth_plugin_msg_t and walks the standard
 * Ethereum-app plugin lifecycle (INIT → PROVIDE_PARAMETER ×N →
 * FINALIZE → PROVIDE_INFO → QUERY_CONTRACT_ID / QUERY_CONTRACT_UI).
 *
 * Two recent security fixes drive parts of this suite:
 *   - 816010e4 ("Zero ERC-20 plugin context and require both ABI
 *     fields before review") — INIT now explicit_bzeroes the whole
 *     context, and FINALIZE refuses unless BOTH destination_parsed
 *     and amount_parsed are true. Regression tests pin both.
 *   - PR #1038 ("ERC20 swap flow accepts approve() as a valid
 *     transfer") — title is slightly misleading: in swap mode the
 *     plugin now REJECTS approve() and any extra_data. Two
 *     regression tests pin both rejections.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "erc20_plugin.h"
#include "eth_plugin_interface.h"
#include "eth_plugin_internal.h"
#include "shared_context.h"
#include "token_info.h"
#include "eth_swap_utils.h"
#include "wraps.h"
#include "Mocknetwork.h"

// =============================================================================
// erc20_parameters_t mirror
// =============================================================================
// The real struct is declared `static`-equivalent inside erc20_plugin.c (no
// public header). Mirror its layout here so the tests can poke fields by
// name. If the production struct ever changes, this declaration must be
// updated in lockstep — there is no compile-time guard against drift.

#define MAX_CONTRACT_NAME_LEN_TEST 15
#define MAX_EXTRA_DATA_CHUNKS_TEST 2

typedef struct {
    uint8_t selectorIndex;
    uint8_t destinationAddress[ADDRESS_LENGTH];
    uint8_t amount[INT256_LENGTH];
    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;
    char contract_name[MAX_CONTRACT_NAME_LEN_TEST];
    char extra_data[MAX_EXTRA_DATA_CHUNKS_TEST * CALLDATA_CHUNK_SIZE];
    uint8_t extra_data_len;
    bool destination_parsed;
    bool amount_parsed;
} erc20_parameters_t;

// =============================================================================
// Globals the module reads
// =============================================================================

pluginType_t pluginType = PLUGIN_TYPE_OLD_INTERNAL;

// =============================================================================
// Wraps
// =============================================================================

static const s_token_info *g_token_info_ret = NULL;
const s_token_info *get_matching_token_info(const uint64_t *chain_id, const uint8_t *addr) {
    (void) chain_id;
    (void) addr;
    return g_token_info_ret;
}

// swap_check_* abort the app on mismatch. For tests we want them to be
// observable no-ops so the FINALIZE swap branch can be exercised end-
// to-end without hitting app_exit().
static int g_swap_check_dest_calls = 0;
static int g_swap_check_amount_calls = 0;
bool swap_check_destination(const char *destination) {
    (void) destination;
    g_swap_check_dest_calls++;
    return true;
}
bool swap_check_amount(const char *amount) {
    (void) amount;
    g_swap_check_amount_calls++;
    return true;
}

// =============================================================================
// Fixtures
// =============================================================================

static erc20_parameters_t g_ctx;

// Selectors copied from erc20_plugin.c (a duplicate but the source is private).
static const uint8_t TRANSFER_SEL[CALLDATA_SELECTOR_SIZE] = {0xa9, 0x05, 0x9c, 0xbb};
static const uint8_t APPROVE_SEL[CALLDATA_SELECTOR_SIZE] = {0x09, 0x5e, 0xa7, 0xb3};
static const uint8_t UNKNOWN_SEL[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

static void reset(void) {
    memset(&g_tx_content, 0, sizeof(g_tx_content));
    memset(&g_ctx, 0, sizeof(g_ctx));
    memset(&strings, 0, sizeof(strings));
    G_called_from_swap = false;
    G_swap_checked = false;
    g_token_info_ret = NULL;
    g_swap_check_dest_calls = 0;
    g_swap_check_amount_calls = 0;
}

static void init_with_selector(const uint8_t *selector) {
    ethPluginInitContract_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.selector = selector;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
}

// =============================================================================
// ETH_PLUGIN_INIT_CONTRACT
// =============================================================================

void test_init_transfer_selector_ok(void) {
    // 816010e4 defense — pre-pollute the context to verify INIT wipes it
    memset(g_ctx.destinationAddress, 0xAA, ADDRESS_LENGTH);
    memset(g_ctx.amount, 0xBB, INT256_LENGTH);
    g_ctx.destination_parsed = true;
    g_ctx.amount_parsed = true;

    ethPluginInitContract_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.selector = TRANSFER_SEL;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(g_ctx.selectorIndex, 0);  // ERC20_TRANSFER
    // The pre-existing pollution must be gone.
    static const uint8_t zero_addr[ADDRESS_LENGTH] = {0};
    TEST_ASSERT_EQUAL_MEMORY(g_ctx.destinationAddress, zero_addr, ADDRESS_LENGTH);
    TEST_ASSERT_FALSE(g_ctx.destination_parsed);
    TEST_ASSERT_FALSE(g_ctx.amount_parsed);
}

void test_init_approve_selector_ok(void) {
    init_with_selector(APPROVE_SEL);
    // selectorIndex = 1 after wipe + match on APPROVE.
    // result is set on the local msg, so we re-call with explicit msg here:
    ethPluginInitContract_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.selector = APPROVE_SEL;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(g_ctx.selectorIndex, 1);  // ERC20_APPROVE
}

void test_init_unknown_selector_rejected(void) {
    ethPluginInitContract_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.selector = UNKNOWN_SEL;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_init_nonzero_tx_value_rejected(void) {
    // The plugin enforces ETH amount == 0 (an ERC-20 call should never
    // also send native value).
    g_tx_content.value.value[31] = 0x01;
    ethPluginInitContract_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.selector = TRANSFER_SEL;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// ETH_PLUGIN_PROVIDE_PARAMETER
// =============================================================================

void test_provide_destination_parameter(void) {
    ethPluginProvideParameter_t msg = {0};
    uint8_t param[CALLDATA_CHUNK_SIZE] = {0};
    // Address is right-aligned in the 32-byte param chunk.
    for (int i = 0; i < ADDRESS_LENGTH; ++i) {
        param[CALLDATA_CHUNK_SIZE - ADDRESS_LENGTH + i] = (uint8_t) (i + 0x10);
    }
    msg.parameter = param;
    msg.parameterOffset = CALLDATA_SELECTOR_SIZE;
    msg.parameter_size = CALLDATA_CHUNK_SIZE;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_TRUE(g_ctx.destination_parsed);
    for (int i = 0; i < ADDRESS_LENGTH; ++i) {
        TEST_ASSERT_EQUAL(g_ctx.destinationAddress[i], (uint8_t) (i + 0x10));
    }
}

void test_provide_amount_parameter(void) {
    ethPluginProvideParameter_t msg = {0};
    uint8_t param[CALLDATA_CHUNK_SIZE];
    for (int i = 0; i < CALLDATA_CHUNK_SIZE; ++i) param[i] = (uint8_t) i;
    msg.parameter = param;
    msg.parameterOffset = CALLDATA_SELECTOR_SIZE + CALLDATA_CHUNK_SIZE;
    msg.parameter_size = CALLDATA_CHUNK_SIZE;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_TRUE(g_ctx.amount_parsed);
    TEST_ASSERT_EQUAL_MEMORY(g_ctx.amount, param, CALLDATA_CHUNK_SIZE);
}

void test_provide_extra_data_parameter(void) {
    ethPluginProvideParameter_t msg = {0};
    uint8_t param[CALLDATA_CHUNK_SIZE];
    for (int i = 0; i < CALLDATA_CHUNK_SIZE; ++i) param[i] = (uint8_t) (i + 0x40);
    msg.parameter = param;
    msg.parameterOffset = CALLDATA_SELECTOR_SIZE + (CALLDATA_CHUNK_SIZE * 2);
    msg.parameter_size = CALLDATA_CHUNK_SIZE;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(g_ctx.extra_data_len, CALLDATA_CHUNK_SIZE);
    TEST_ASSERT_EQUAL_MEMORY(g_ctx.extra_data, param, CALLDATA_CHUNK_SIZE);
}

void test_provide_extra_data_overflow_rejected(void) {
    ethPluginProvideParameter_t msg = {0};
    uint8_t param[CALLDATA_CHUNK_SIZE] = {0};
    // Offset just past the MAX_EXTRA_DATA_CHUNKS (2) window.
    msg.parameter = param;
    msg.parameterOffset = CALLDATA_SELECTOR_SIZE + CALLDATA_CHUNK_SIZE + (3 * CALLDATA_CHUNK_SIZE);
    msg.parameter_size = CALLDATA_CHUNK_SIZE;
    msg.pluginContext = (uint8_t *) &g_ctx;
    g_ctx.extra_data_len = 5;  // pre-existing
    erc20_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
    // The plugin resets extra_data_len when it refuses the overflow.
    TEST_ASSERT_EQUAL(g_ctx.extra_data_len, 0);
}

// =============================================================================
// ETH_PLUGIN_FINALIZE — 816010e4 "both required" guard
// =============================================================================

void test_finalize_missing_destination_rejected(void) {
    g_ctx.amount_parsed = true;
    // destination_parsed stays false
    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_finalize_missing_amount_rejected(void) {
    g_ctx.destination_parsed = true;
    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_finalize_both_present_non_swap_ok(void) {
    g_ctx.destination_parsed = true;
    g_ctx.amount_parsed = true;
    G_called_from_swap = false;

    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(msg.uiType, ETH_UI_TYPE_GENERIC);
    TEST_ASSERT_EQUAL(msg.numScreens, 2);
    TEST_ASSERT_EQUAL_PTR(msg.tokenLookup1, g_tx_content.destination);
}

void test_finalize_with_extra_data_bumps_numscreens(void) {
    g_ctx.destination_parsed = true;
    g_ctx.amount_parsed = true;
    g_ctx.extra_data_len = 10;

    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(msg.numScreens, 3);
}

// =============================================================================
// ETH_PLUGIN_FINALIZE — PR #1038 swap-mode restrictions
// =============================================================================

void test_finalize_swap_approve_rejected(void) {
    g_ctx.destination_parsed = true;
    g_ctx.amount_parsed = true;
    g_ctx.selectorIndex = 1;  // ERC20_APPROVE
    G_called_from_swap = true;

    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
    // swap_check_destination must not have been called — the early
    // selector reject runs first.
    TEST_ASSERT_EQUAL(g_swap_check_dest_calls, 0);
}

void test_finalize_swap_with_extra_data_rejected(void) {
    g_ctx.destination_parsed = true;
    g_ctx.amount_parsed = true;
    g_ctx.selectorIndex = 0;   // ERC20_TRANSFER
    g_ctx.extra_data_len = 4;  // any non-zero
    G_called_from_swap = true;

    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
    TEST_ASSERT_EQUAL(g_swap_check_dest_calls, 0);
}

void test_finalize_swap_transfer_no_extra_runs_swap_checks(void) {
    g_ctx.destination_parsed = true;
    g_ctx.amount_parsed = true;
    g_ctx.selectorIndex = 0;  // ERC20_TRANSFER
    g_ctx.extra_data_len = 0;
    G_called_from_swap = true;

    // token_info lookup must succeed for amountToString to run.
    static s_token_info info = {.decimals = 6, .chain_id = 1};
    strlcpy(info.ticker, "USDC", sizeof(info.ticker));
    g_token_info_ret = &info;

    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(g_swap_check_dest_calls, 1);
    TEST_ASSERT_EQUAL(g_swap_check_amount_calls, 1);
    TEST_ASSERT_TRUE(G_swap_checked);
}

void test_finalize_swap_token_info_missing_rejected(void) {
    g_ctx.destination_parsed = true;
    g_ctx.amount_parsed = true;
    g_ctx.selectorIndex = 0;
    G_called_from_swap = true;
    g_token_info_ret = NULL;

    ethPluginFinalize_t msg = {0};
    msg.txContent = &g_tx_content;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
    // swap_check_destination was called first (it precedes the lookup),
    // swap_check_amount was NOT reached.
    TEST_ASSERT_EQUAL(g_swap_check_dest_calls, 1);
    TEST_ASSERT_EQUAL(g_swap_check_amount_calls, 0);
}

// =============================================================================
// ETH_PLUGIN_PROVIDE_INFO
// =============================================================================

void test_provide_info_item1_ok(void) {
    static const tokenDefinition_t token = {.ticker = "DAI", .decimals = 18};
    static union extraInfo_t item;
    memcpy(&item.token, &token, sizeof(token));

    ethPluginProvideInfo_t msg = {0};
    msg.item1 = &item;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(g_ctx.ticker, "DAI");
    TEST_ASSERT_EQUAL(g_ctx.decimals, 18);
}

void test_provide_info_item1_null_falls_back(void) {
    ethPluginProvideInfo_t msg = {0};
    msg.item1 = NULL;
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_FALLBACK);
}

// =============================================================================
// ETH_PLUGIN_QUERY_CONTRACT_ID
// =============================================================================

void test_query_contract_id_transfer(void) {
    char name[32] = {0};
    char version[32] = {0};
    g_ctx.selectorIndex = 0;  // ERC20_TRANSFER

    ethQueryContractID_t msg = {0};
    msg.name = name;
    msg.version = version;
    msg.nameLength = sizeof(name);
    msg.versionLength = sizeof(version);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(name, "ERC20 token");
    TEST_ASSERT_EQUAL_STRING(version, "Send");
}

void test_query_contract_id_approve(void) {
    char name[32] = {0};
    char version[32] = {0};
    g_ctx.selectorIndex = 1;  // ERC20_APPROVE

    ethQueryContractID_t msg = {0};
    msg.name = name;
    msg.version = version;
    msg.nameLength = sizeof(name);
    msg.versionLength = sizeof(version);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL_STRING(version, "Approve");
}

// =============================================================================
// ETH_PLUGIN_QUERY_CONTRACT_UI — screen 0 (amount/title)
// =============================================================================

void test_query_ui_screen0_transfer_amount(void) {
    char title[16] = {0};
    char msgbuf[64] = {0};
    g_ctx.selectorIndex = 0;
    g_ctx.amount[31] = 0x05;  // small amount 5
    g_ctx.decimals = 0;
    strlcpy(g_ctx.ticker, "USDC", sizeof(g_ctx.ticker));

    ethQueryContractUI_t msg = {0};
    msg.screenIndex = 0;
    msg.title = title;
    msg.msg = msgbuf;
    msg.titleLength = sizeof(title);
    msg.msgLength = sizeof(msgbuf);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "Send");
    TEST_ASSERT_EQUAL_STRING(msgbuf, "5 USDC");
}

void test_query_ui_screen0_approve_unlimited(void) {
    char title[16] = {0};
    char msgbuf[64] = {0};
    g_ctx.selectorIndex = 1;                           // APPROVE
    memset(g_ctx.amount, 0xFF, sizeof(g_ctx.amount));  // max-int → "Unlimited"
    strlcpy(g_ctx.ticker, "DAI", sizeof(g_ctx.ticker));

    ethQueryContractUI_t msg = {0};
    msg.screenIndex = 0;
    msg.title = title;
    msg.msg = msgbuf;
    msg.titleLength = sizeof(title);
    msg.msgLength = sizeof(msgbuf);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);

    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "Approve");
    TEST_ASSERT_EQUAL_STRING(msgbuf, "Unlimited DAI");
}

// =============================================================================
// ETH_PLUGIN_QUERY_CONTRACT_UI — screens 1 and 2 (To / Approve to /
// Extra Data) and the FINALIZE swap-mode error paths.
//
// These paths render the destination address the user is about to
// approve a token transfer/allowance to. A regression here turns the
// approval scam vector live — the user signs whatever the plugin
// happens to put on screen.
// =============================================================================

void test_query_ui_screen1_transfer_renders_to(void) {
    char title[16] = {0};
    char msgbuf[64] = {0};
    g_ctx.selectorIndex = 0;  // TRANSFER
    memset(g_ctx.destinationAddress, 0xAB, ADDRESS_LENGTH);

    ethQueryContractUI_t msg = {0};
    msg.screenIndex = 1;
    msg.title = title;
    msg.msg = msgbuf;
    msg.titleLength = sizeof(title);
    msg.msgLength = sizeof(msgbuf);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "To");
    // The mock getEthDisplayableAddress in the test environment writes
    // a checksummed hex string to msg — we only assert it's non-empty.
    TEST_ASSERT_TRUE(msgbuf[0] != '\0');
}

void test_query_ui_screen1_approve_renders_approve_to(void) {
    char title[16] = {0};
    char msgbuf[64] = {0};
    g_ctx.selectorIndex = 1;  // APPROVE
    memset(g_ctx.destinationAddress, 0xCD, ADDRESS_LENGTH);

    ethQueryContractUI_t msg = {0};
    msg.screenIndex = 1;
    msg.title = title;
    msg.msg = msgbuf;
    msg.titleLength = sizeof(title);
    msg.msgLength = sizeof(msgbuf);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "Approve to");
}

// Screen 2 — Extra Data, ASCII-printable path.
void test_query_ui_screen2_extra_data_printable(void) {
    char title[16] = {0};
    char msgbuf[64] = {0};
    g_ctx.selectorIndex = 0;
    strlcpy(g_ctx.extra_data, "Hello!", sizeof(g_ctx.extra_data));
    g_ctx.extra_data_len = 6;

    ethQueryContractUI_t msg = {0};
    msg.screenIndex = 2;
    msg.title = title;
    msg.msg = msgbuf;
    msg.titleLength = sizeof(title);
    msg.msgLength = sizeof(msgbuf);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "Extra Data");
    TEST_ASSERT_EQUAL_STRING(msgbuf, "Hello!");
}

// Screen 2 — Extra Data, non-printable path (rendered as 0x-prefixed hex).
void test_query_ui_screen2_extra_data_hex(void) {
    char title[16] = {0};
    char msgbuf[64] = {0};
    g_ctx.selectorIndex = 0;
    // extra_data is char[] (signed on this target); the byte values above
    // 0x7F have to be assigned through their unsigned bit-pattern via
    // memcpy to avoid -Woverflow on the implicit int->char conversion.
    static const uint8_t non_printable_bytes[] = {0x01, 0xFE, 0xAB};
    memcpy(g_ctx.extra_data, non_printable_bytes, sizeof(non_printable_bytes));
    g_ctx.extra_data_len = sizeof(non_printable_bytes);

    ethQueryContractUI_t msg = {0};
    msg.screenIndex = 2;
    msg.title = title;
    msg.msg = msgbuf;
    msg.titleLength = sizeof(title);
    msg.msgLength = sizeof(msgbuf);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(msgbuf, "0x01FEAB");
}

// Screen 2 with empty extra_data is rejected (the plugin should never
// reach this state — extra_data_len == 0 means no extra screen was
// allocated — but the guard exists and must trip).
void test_query_ui_screen2_empty_extra_data_rejected(void) {
    char title[16] = {0};
    char msgbuf[64] = {0};
    g_ctx.extra_data_len = 0;

    ethQueryContractUI_t msg = {0};
    msg.screenIndex = 2;
    msg.title = title;
    msg.msg = msgbuf;
    msg.titleLength = sizeof(title);
    msg.msgLength = sizeof(msgbuf);
    msg.pluginContext = (uint8_t *) &g_ctx;
    erc20_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocknetwork_Init();
    get_tx_chain_id_IgnoreAndReturn(1);
    reset();
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_transfer_selector_ok);
    RUN_TEST(test_init_approve_selector_ok);
    RUN_TEST(test_init_unknown_selector_rejected);
    RUN_TEST(test_init_nonzero_tx_value_rejected);
    RUN_TEST(test_provide_destination_parameter);
    RUN_TEST(test_provide_amount_parameter);
    RUN_TEST(test_provide_extra_data_parameter);
    RUN_TEST(test_provide_extra_data_overflow_rejected);
    RUN_TEST(test_finalize_missing_destination_rejected);
    RUN_TEST(test_finalize_missing_amount_rejected);
    RUN_TEST(test_finalize_both_present_non_swap_ok);
    RUN_TEST(test_finalize_with_extra_data_bumps_numscreens);
    RUN_TEST(test_finalize_swap_approve_rejected);
    RUN_TEST(test_finalize_swap_with_extra_data_rejected);
    RUN_TEST(test_finalize_swap_transfer_no_extra_runs_swap_checks);
    RUN_TEST(test_finalize_swap_token_info_missing_rejected);
    RUN_TEST(test_provide_info_item1_ok);
    RUN_TEST(test_provide_info_item1_null_falls_back);
    RUN_TEST(test_query_contract_id_transfer);
    RUN_TEST(test_query_contract_id_approve);
    RUN_TEST(test_query_ui_screen0_transfer_amount);
    RUN_TEST(test_query_ui_screen0_approve_unlimited);
    RUN_TEST(test_query_ui_screen1_transfer_renders_to);
    RUN_TEST(test_query_ui_screen1_approve_renders_approve_to);
    RUN_TEST(test_query_ui_screen2_extra_data_printable);
    RUN_TEST(test_query_ui_screen2_extra_data_hex);
    RUN_TEST(test_query_ui_screen2_empty_extra_data_rejected);
    return UNITY_END();
}
