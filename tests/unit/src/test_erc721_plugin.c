/**
 * @file test_erc721_plugin.c
 * @brief Unit tests for the ERC-721 internal plugin at
 *        src/plugins/erc721/{erc721_plugin.c, erc721_provide_parameters.c,
 *        erc721_ui.c}.
 *
 * The ERC-721 plugin parses NFT-related calldata (approve /
 * setApprovalForAll / transferFrom / safeTransferFrom) and extracts
 * the (operator|to, tokenId) tuple that the device displays during
 * signing. Every value the user sees on the review screens comes
 * out of this plugin — a bug in the parameter walk shows the wrong
 * recipient or the wrong NFT to the user.
 *
 * Pin every selector branch and every screen the UI renders:
 *  - the five canonical ERC-721 selectors must be recognised and
 *    seed the correct `next_param` state,
 *  - an unknown selector falls back (the app then refuses to use
 *    this plugin for the tx),
 *  - the parameter walk for each selector reaches NONE exactly when
 *    the right field count has been consumed,
 *  - SAFE_TRANSFER_DATA tolerates extra parameters past TOKEN_ID
 *    (the trailing ABI bytes), while SAFE_TRANSFER / TRANSFER stay
 *    strict,
 *  - FINALIZE: TRANSFER/APPROVE → 4 screens, SET_APPROVAL_FOR_ALL
 *    → 3 screens, an extra ETH value bumps by one screen for
 *    transfers but is forbidden for setApprovalForAll,
 *  - QUERY_CONTRACT_ID and QUERY_CONTRACT_UI write the strings the
 *    user actually sees, and reject unknown selector indices or a
 *    missing NFT info pointer.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "erc721_plugin.h"
#include "erc721_internal.h"
#include "eth_plugin_interface.h"
#include "shared_context.h"
#include "asset_info.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// getEthDisplayableAddress lives in common_utils.c. Wrap it so tests
// see deterministic output without touching keccak.
bool __wrap_getEthDisplayableAddress(const uint8_t *addr,
                                     char *out,
                                     size_t out_size,
                                     uint64_t chain_id) {
    (void) addr;
    (void) chain_id;
    if (out == NULL || out_size < 3) return false;
    strncpy(out, "0xADDR", out_size - 1);
    out[out_size - 1] = '\0';
    return true;
}

// =============================================================================
// ERC-721 canonical selectors (copy of the source for byte-pinning)
// =============================================================================

static const uint8_t SEL_APPROVE[] = {0x09, 0x5e, 0xa7, 0xb3};
static const uint8_t SEL_APPROVE_FOR_ALL[] = {0xa2, 0x2c, 0xb4, 0x65};
static const uint8_t SEL_TRANSFER[] = {0x23, 0xb8, 0x72, 0xdd};
static const uint8_t SEL_SAFE_TRANSFER[] = {0x42, 0x84, 0x2e, 0x0e};
static const uint8_t SEL_SAFE_TRANSFER_DATA[] = {0xb8, 0x8d, 0x4f, 0xde};

static void init_msg_with_selector(ethPluginInitContract_t *msg,
                                   uint8_t *ctx_storage,
                                   txContent_t *tx_content,
                                   const uint8_t *selector) {
    memset(msg, 0, sizeof(*msg));
    memset(tx_content, 0, sizeof(*tx_content));
    msg->pluginContext = ctx_storage;
    msg->pluginContextLength = sizeof(erc721_context_t);
    msg->selector = selector;
    msg->txContent = tx_content;
    msg->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
}

// Helper: build a 32-byte ABI parameter where the last 20 bytes are
// the address (left-padded with zeroes per ABI rules).
static void make_abi_address(uint8_t *param, uint8_t fill) {
    memset(param, 0, PARAMETER_LENGTH);
    memset(param + 12, fill, 20);
}

// =============================================================================
// Tests — INIT
// =============================================================================

void test_init_recognises_approve(void) {
    erc721_context_t ctx = {0};
    ethPluginInitContract_t msg;
    txContent_t tx;
    init_msg_with_selector(&msg, (uint8_t *) &ctx, &tx, SEL_APPROVE);
    erc721_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, APPROVE);
    TEST_ASSERT_EQUAL(ctx.next_param, OPERATOR);
}

void test_init_recognises_approve_for_all(void) {
    erc721_context_t ctx = {0};
    ethPluginInitContract_t msg;
    txContent_t tx;
    init_msg_with_selector(&msg, (uint8_t *) &ctx, &tx, SEL_APPROVE_FOR_ALL);
    erc721_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, SET_APPROVAL_FOR_ALL);
    TEST_ASSERT_EQUAL(ctx.next_param, OPERATOR);
}

void test_init_recognises_transfer(void) {
    erc721_context_t ctx = {0};
    ethPluginInitContract_t msg;
    txContent_t tx;
    init_msg_with_selector(&msg, (uint8_t *) &ctx, &tx, SEL_TRANSFER);
    erc721_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, TRANSFER);
    TEST_ASSERT_EQUAL(ctx.next_param, FROM);
}

void test_init_recognises_safe_transfer(void) {
    erc721_context_t ctx = {0};
    ethPluginInitContract_t msg;
    txContent_t tx;
    init_msg_with_selector(&msg, (uint8_t *) &ctx, &tx, SEL_SAFE_TRANSFER);
    erc721_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, SAFE_TRANSFER);
}

void test_init_recognises_safe_transfer_data(void) {
    erc721_context_t ctx = {0};
    ethPluginInitContract_t msg;
    txContent_t tx;
    init_msg_with_selector(&msg, (uint8_t *) &ctx, &tx, SEL_SAFE_TRANSFER_DATA);
    erc721_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, SAFE_TRANSFER_DATA);
}

void test_init_unknown_selector_falls_back(void) {
    erc721_context_t ctx = {0};
    ethPluginInitContract_t msg;
    txContent_t tx;
    static const uint8_t unknown[] = {0xFF, 0xFF, 0xFF, 0xFF};
    init_msg_with_selector(&msg, (uint8_t *) &ctx, &tx, unknown);
    erc721_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_FALLBACK);
}

void test_init_zeroes_stale_context(void) {
    erc721_context_t ctx;
    memset(&ctx, 0xCC, sizeof(ctx));
    ethPluginInitContract_t msg;
    txContent_t tx;
    init_msg_with_selector(&msg, (uint8_t *) &ctx, &tx, SEL_APPROVE);
    erc721_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, APPROVE);
    // explicit_bzero must have cleared everything before selectorIndex was written.
    TEST_ASSERT_EQUAL(ctx.next_param, OPERATOR);
    TEST_ASSERT_EQUAL(ctx.approved, 0);
}

// =============================================================================
// Tests — PROVIDE_PARAMETER
// =============================================================================

void test_approve_param_walk(void) {
    erc721_context_t ctx = {.selectorIndex = APPROVE, .next_param = OPERATOR};
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    uint8_t param[PARAMETER_LENGTH];
    msg.parameter = param;

    // 1st parameter: operator address.
    make_abi_address(param, 0xAA);
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_SUCCESSFUL);
    TEST_ASSERT_EQUAL(ctx.next_param, TOKEN_ID);
    for (int i = 0; i < 20; i++) TEST_ASSERT_EQUAL(ctx.address[i], 0xAA);

    // 2nd parameter: token id.
    memset(param, 0x11, sizeof(param));
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_SUCCESSFUL);
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);
    for (int i = 0; i < INT256_LENGTH; i++) TEST_ASSERT_EQUAL(ctx.tokenId[i], 0x11);

    // 3rd parameter: should NOT be accepted (default arm of switch).
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_transfer_param_walk_strict(void) {
    erc721_context_t ctx = {.selectorIndex = TRANSFER, .next_param = FROM};
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    uint8_t param[PARAMETER_LENGTH];
    msg.parameter = param;

    // FROM (consumed, not stored)
    make_abi_address(param, 0xBB);
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(ctx.next_param, TO);

    // TO (stored in address)
    make_abi_address(param, 0xCC);
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(ctx.next_param, TOKEN_ID);
    for (int i = 0; i < 20; i++) TEST_ASSERT_EQUAL(ctx.address[i], 0xCC);

    // TOKEN_ID
    memset(param, 0x42, sizeof(param));
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);

    // Extra param under strict mode → error.
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_safe_transfer_data_tolerates_extra_params(void) {
    erc721_context_t ctx = {.selectorIndex = SAFE_TRANSFER_DATA, .next_param = FROM};
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    uint8_t param[PARAMETER_LENGTH];
    msg.parameter = param;

    make_abi_address(param, 0xAA);
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);  // FROM
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);  // TO
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);  // TOKEN_ID
    // The bytes payload overhead is walked before NONE
    TEST_ASSERT_EQUAL(ctx.next_param, DATA_OFFSET);

    // Dynamic offset of the payload, counted from after the selector
    memset(param, 0, sizeof(param));
    param[PARAMETER_LENGTH - 1] = 4 * PARAMETER_LENGTH;
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);  // DATA_OFFSET
    TEST_ASSERT_EQUAL(ctx.next_param, DATA_LENGTH);

    // The length word lands at the declared offset
    msg.parameterOffset = SELECTOR_SIZE + 4 * PARAMETER_LENGTH;
    memset(param, 0, sizeof(param));
    param[PARAMETER_LENGTH - 1] = 9;
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);  // DATA_LENGTH
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);

    // Non-strict: extra param after NONE is ignored, not an error.
    msg.result = ETH_PLUGIN_RESULT_SUCCESSFUL;
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_SUCCESSFUL);
}

void test_approval_for_all_walks_two_params(void) {
    erc721_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .next_param = OPERATOR};
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    uint8_t param[PARAMETER_LENGTH];
    msg.parameter = param;

    // OPERATOR.
    make_abi_address(param, 0xDD);
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(ctx.next_param, APPROVED);
    for (int i = 0; i < 20; i++) TEST_ASSERT_EQUAL(ctx.address[i], 0xDD);

    // APPROVED (bool: last byte of the 32-byte param).
    memset(param, 0, sizeof(param));
    param[PARAMETER_LENGTH - 1] = 0x01;
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);
    TEST_ASSERT_TRUE(ctx.approved);

    // Extra param → error.
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_unknown_selector_index_rejects(void) {
    erc721_context_t ctx = {.selectorIndex = 0x7F};
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    uint8_t param[PARAMETER_LENGTH] = {0};
    msg.parameter = param;
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Tests — FINALIZE
// =============================================================================

void test_finalize_transfer_five_screens(void) {
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc721_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(msg.numScreens, 5);
    TEST_ASSERT_EQUAL(msg.uiType, ETH_UI_TYPE_GENERIC);
}

void test_finalize_set_approval_three_screens(void) {
    erc721_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc721_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(msg.numScreens, 3);
}

void test_finalize_transfer_with_eth_value_adds_screen(void) {
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    txContent_t tx = {0};
    tx.value.value[0] = 0x01;  // non-zero ETH
    tx.value.length = 1;
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc721_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(msg.numScreens, 6);
}

void test_finalize_set_approval_with_eth_rejected(void) {
    erc721_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL};
    txContent_t tx = {0};
    tx.value.value[0] = 0x01;
    tx.value.length = 1;
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc721_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    // setApprovalForAll is non-payable. Sending ETH alongside it
    // means the calldata is hostile / malformed.
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Tests — QUERY_CONTRACT_ID
// =============================================================================

void test_query_contract_id_approve(void) {
    erc721_context_t ctx = {.selectorIndex = APPROVE};
    char name[32] = {0};
    char version[16] = {0};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(name, "NFT allowance");
    TEST_ASSERT_EQUAL_STRING(version, "manage");
}

void test_query_contract_id_transfer(void) {
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    char name[32] = {0};
    char version[16] = {0};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(name, "NFT");
    TEST_ASSERT_EQUAL_STRING(version, "Transfer");
}

void test_finalize_unknown_selector_rejects(void) {
    erc721_context_t ctx = {.selectorIndex = 0x7F};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc721_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_query_id_unknown_selector_rejects(void) {
    erc721_context_t ctx = {.selectorIndex = 0x7F};
    char name[32] = {0};
    char version[32] = {0};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_provide_info_returns_ok(void) {
    // handle_provide_info_721 just sets result = OK; pin the dispatch
    // path through erc721_plugin_call too.
    ethPluginProvideInfo_t msg = {0};
    erc721_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
}

// =============================================================================
// Tests — QUERY_CONTRACT_UI
// =============================================================================

static union extraInfo_t g_nft_info;
static void prep_nft_info(void) {
    memset(&g_nft_info, 0, sizeof(g_nft_info));
    strncpy(g_nft_info.nft.collectionName,
            "CryptoPunks",
            sizeof(g_nft_info.nft.collectionName) - 1);
    memset(g_nft_info.nft.contractAddress, 0xBB, ADDRESS_LENGTH);
}

static void prep_ui_msg(ethQueryContractUI_t *msg,
                        erc721_context_t *ctx,
                        char *title,
                        char *body,
                        uint8_t screen) {
    memset(msg, 0, sizeof(*msg));
    msg->pluginContext = (uint8_t *) ctx;
    msg->item1 = &g_nft_info;
    msg->title = title;
    msg->titleLength = 32;
    msg->msg = body;
    msg->msgLength = 64;
    msg->screenIndex = screen;
}

void test_ui_null_item1_rejected(void) {
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    char title[32] = {0};
    char body[64] = {0};
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.item1 = NULL;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_ui_transfer_screen0_is_owner_address(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 0);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Owner");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_transfer_screen1_is_to_address(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 1);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "To");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_transfer_screen2_is_collection(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 2);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Collection Name");
    TEST_ASSERT_EQUAL_STRING(body, "CryptoPunks");
}

void test_ui_transfer_screen4_is_nft_id(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    // tokenId = 42 in big-endian (encoded by copy_parameter on a
    // 32-byte ABI parameter where the LSB is at offset 31).
    ctx.tokenId[31] = 42;
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 4);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT ID");
    TEST_ASSERT_EQUAL_STRING(body, "42");
}

void test_ui_transfer_unknown_screen_rejected(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 99);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_ui_approval_for_all_allow_vs_revoke(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 0);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Allow");

    ctx.approved = false;
    prep_ui_msg(&msg, &ctx, title, body, 0);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Revoke");
}

void test_ui_approve_screen0_is_allow_to_address(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = APPROVE};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 0);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Allow");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

// Remaining sub-screens for each selector: TRANSFER screen 2, APPROVE
// screens 1/2/3, SET_APPROVAL_FOR_ALL screens 1/2, plus the default
// arms in each switch.

void test_ui_transfer_screen3_is_nft_address(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 3);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Address");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_approve_screen1_is_to_manage_collection(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = APPROVE};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 1);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "To Manage Your");
    TEST_ASSERT_EQUAL_STRING(body, "CryptoPunks");
}

void test_ui_approve_screen2_is_nft_address(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = APPROVE};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 2);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Address");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_approve_screen3_is_nft_id(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = APPROVE};
    ctx.tokenId[31] = 7;
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 3);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "NFT ID");
    TEST_ASSERT_EQUAL_STRING(body, "7");
}

void test_ui_approve_unknown_screen_rejected(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = APPROVE};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 99);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_ui_approval_for_all_screen1_is_to_manage_all(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 1);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "To Manage ALL");
    TEST_ASSERT_EQUAL_STRING(body, "CryptoPunks");
}

void test_ui_approval_for_all_screen2_is_nft_address(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 2);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Address");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_approval_for_all_unknown_screen_rejected(void) {
    prep_nft_info();
    erc721_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 99);
    erc721_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_recognises_approve);
    RUN_TEST(test_init_recognises_approve_for_all);
    RUN_TEST(test_init_recognises_transfer);
    RUN_TEST(test_init_recognises_safe_transfer);
    RUN_TEST(test_init_recognises_safe_transfer_data);
    RUN_TEST(test_init_unknown_selector_falls_back);
    RUN_TEST(test_init_zeroes_stale_context);
    RUN_TEST(test_approve_param_walk);
    RUN_TEST(test_transfer_param_walk_strict);
    RUN_TEST(test_safe_transfer_data_tolerates_extra_params);
    RUN_TEST(test_approval_for_all_walks_two_params);
    RUN_TEST(test_unknown_selector_index_rejects);
    RUN_TEST(test_finalize_transfer_five_screens);
    RUN_TEST(test_finalize_set_approval_three_screens);
    RUN_TEST(test_finalize_transfer_with_eth_value_adds_screen);
    RUN_TEST(test_finalize_set_approval_with_eth_rejected);
    RUN_TEST(test_query_contract_id_approve);
    RUN_TEST(test_query_contract_id_transfer);
    RUN_TEST(test_finalize_unknown_selector_rejects);
    RUN_TEST(test_query_id_unknown_selector_rejects);
    RUN_TEST(test_provide_info_returns_ok);
    RUN_TEST(test_ui_null_item1_rejected);
    RUN_TEST(test_ui_transfer_screen0_is_owner_address);
    RUN_TEST(test_ui_transfer_screen1_is_to_address);
    RUN_TEST(test_ui_transfer_screen2_is_collection);
    RUN_TEST(test_ui_transfer_screen4_is_nft_id);
    RUN_TEST(test_ui_transfer_unknown_screen_rejected);
    RUN_TEST(test_ui_approval_for_all_allow_vs_revoke);
    RUN_TEST(test_ui_approve_screen0_is_allow_to_address);
    RUN_TEST(test_ui_transfer_screen3_is_nft_address);
    RUN_TEST(test_ui_approve_screen1_is_to_manage_collection);
    RUN_TEST(test_ui_approve_screen2_is_nft_address);
    RUN_TEST(test_ui_approve_screen3_is_nft_id);
    RUN_TEST(test_ui_approve_unknown_screen_rejected);
    RUN_TEST(test_ui_approval_for_all_screen1_is_to_manage_all);
    RUN_TEST(test_ui_approval_for_all_screen2_is_nft_address);
    RUN_TEST(test_ui_approval_for_all_unknown_screen_rejected);
    return UNITY_END();
}
