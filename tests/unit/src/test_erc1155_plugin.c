/**
 * @file test_erc1155_plugin.c
 * @brief Unit tests for the ERC-1155 internal plugin at
 *        src/plugins/erc1155/{erc1155_plugin.c,
 *        erc1155_provide_parameters.c, erc1155_ui.c}.
 *
 * ERC-1155 is the multi-token standard: a single batch transfer can
 * move many distinct (tokenId, value) pairs at once. The plugin
 * parses the ABI-encoded calldata into the context the UI then
 * renders, and enforces several invariants that the host cannot be
 * trusted to honour:
 *  - the offsets declared in the calldata head must align with where
 *    the parser sees the array length bytes (defence against
 *    spoofed offsets that point past the real array, leaving the
 *    parser to read uninitialised bytes),
 *  - the tokenIds[] and values[] arrays must have the same length
 *    (an attacker could otherwise hide values past the visible IDs),
 *  - the aggregate "total quantity" shown to the user is the
 *    arithmetic sum of every per-id value, with a uint256-overflow
 *    guard so a crafted batch cannot silently misreport that total,
 *  - the first ERC1155_BATCH_DISPLAY_MAX pairs are captured for the
 *    detail screens so a high-value tokenId cannot hide among
 *    innocuous ones (only the aggregate would otherwise have been
 *    shown),
 *  - any non-zero ETH value attached to the tx is rejected since
 *    none of the three selectors are payable.
 *
 * Pin every one of these.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "erc1155_plugin.h"
#include "erc1155_internal.h"
#include "eth_plugin_interface.h"
#include "shared_context.h"
#include "asset_info.h"
#include "uint256.h"
#include "uint_common.h"
#include "common_utils.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Helpers
// =============================================================================

bool __wrap_getEthDisplayableAddress(const uint8_t *addr,
                                     char *out,
                                     size_t out_size,
                                     uint64_t chain) {
    (void) addr;
    (void) chain;
    if (out == NULL || out_size < 3) return false;
    strncpy(out, "0xADDR", out_size - 1);
    out[out_size - 1] = '\0';
    return true;
}

// Build a 32-byte ABI parameter from a u32 (right-aligned BE).
static void make_abi_u32(uint8_t *param, uint32_t v) {
    memset(param, 0, PARAMETER_LENGTH);
    param[PARAMETER_LENGTH - 4] = (uint8_t) (v >> 24);
    param[PARAMETER_LENGTH - 3] = (uint8_t) (v >> 16);
    param[PARAMETER_LENGTH - 2] = (uint8_t) (v >> 8);
    param[PARAMETER_LENGTH - 1] = (uint8_t) v;
}

static void make_abi_address(uint8_t *param, uint8_t fill) {
    memset(param, 0, PARAMETER_LENGTH);
    memset(param + 12, fill, 20);
}

// =============================================================================
// Selectors
// =============================================================================

static const uint8_t SEL_APPROVE_FOR_ALL[] = {0xa2, 0x2c, 0xb4, 0x65};
static const uint8_t SEL_SAFE_TRANSFER[] = {0xf2, 0x42, 0x43, 0x2a};
static const uint8_t SEL_SAFE_BATCH[] = {0x2e, 0xb2, 0xc2, 0xd6};

static void run_init(erc1155_context_t *ctx, const uint8_t *selector) {
    txContent_t tx = {0};
    ethPluginInitContract_t msg = {0};
    msg.pluginContext = (uint8_t *) ctx;
    msg.pluginContextLength = sizeof(*ctx);
    msg.selector = selector;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
}

static void feed_param(erc1155_context_t *ctx, uint8_t *param, uint32_t offset) {
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) ctx;
    msg.parameter = param;
    msg.parameterOffset = offset;
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
}

// =============================================================================
// Tests — INIT
// =============================================================================

void test_init_recognises_three_selectors(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_APPROVE_FOR_ALL);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, SET_APPROVAL_FOR_ALL);
    TEST_ASSERT_EQUAL(ctx.next_param, OPERATOR);

    memset(&ctx, 0, sizeof(ctx));
    run_init(&ctx, SEL_SAFE_TRANSFER);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, SAFE_TRANSFER);
    TEST_ASSERT_EQUAL(ctx.next_param, FROM);

    memset(&ctx, 0, sizeof(ctx));
    run_init(&ctx, SEL_SAFE_BATCH);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, SAFE_BATCH_TRANSFER);
    TEST_ASSERT_EQUAL(ctx.next_param, FROM);
}

void test_init_unknown_selector_falls_back(void) {
    erc1155_context_t ctx = {0};
    static const uint8_t unknown[] = {0xFF, 0xFF, 0xFF, 0xFF};
    txContent_t tx = {0};
    ethPluginInitContract_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.pluginContextLength = sizeof(ctx);
    msg.selector = unknown;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_INIT_CONTRACT, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_FALLBACK);
}

// =============================================================================
// Tests — PROVIDE_PARAMETER safeTransferFrom
// =============================================================================

void test_init_zeroes_stale_context(void) {
    erc1155_context_t ctx;
    memset(&ctx, 0xCC, sizeof(ctx));
    run_init(&ctx, SEL_SAFE_BATCH);
    TEST_ASSERT_EQUAL(ctx.selectorIndex, SAFE_BATCH_TRANSFER);
    TEST_ASSERT_EQUAL(ctx.next_param, FROM);
    TEST_ASSERT_EQUAL(ctx.batch_displayed, 0);
    TEST_ASSERT_FALSE(ctx.batch_truncated);
}

// safeTransferFrom(from, to, id, value, data): head of 5 words, then the
// data length at offset_to_data. Offsets fed to the plugin include the selector.
static void run_safe_transfer(erc1155_context_t *ctx, uint32_t data_length) {
    uint8_t param[PARAMETER_LENGTH];
    make_abi_address(param, 0xAA);
    feed_param(ctx, param, 4);  // FROM
    make_abi_address(param, 0xBB);
    feed_param(ctx, param, 36);  // TO
    make_abi_u32(param, 42);
    feed_param(ctx, param, 68);  // TOKEN_ID
    make_abi_u32(param, 7);
    feed_param(ctx, param, 100);  // VALUE
    make_abi_u32(param, 160);
    feed_param(ctx, param, 132);  // DATA_OFFSET
    make_abi_u32(param, data_length);
    feed_param(ctx, param, 164);  // DATA_LENGTH
}

void test_safe_transfer_walk(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_TRANSFER);

    uint8_t param[PARAMETER_LENGTH];
    make_abi_address(param, 0xAA);
    feed_param(&ctx, param, 4);  // FROM
    TEST_ASSERT_EQUAL(ctx.next_param, TO);

    make_abi_address(param, 0xBB);
    feed_param(&ctx, param, 36);  // TO
    TEST_ASSERT_EQUAL(ctx.next_param, TOKEN_ID);
    for (int i = 0; i < 20; i++) TEST_ASSERT_EQUAL(ctx.address[i], 0xBB);

    make_abi_u32(param, 42);
    feed_param(&ctx, param, 68);  // TOKEN_ID
    TEST_ASSERT_EQUAL(ctx.next_param, VALUE);

    make_abi_u32(param, 7);
    feed_param(&ctx, param, 100);  // VALUE
    TEST_ASSERT_EQUAL(ctx.next_param, DATA_OFFSET);
    TEST_ASSERT_EQUAL(LOWER(LOWER(ctx.value)), 7);

    make_abi_u32(param, 160);
    feed_param(&ctx, param, 132);  // DATA_OFFSET
    TEST_ASSERT_EQUAL(ctx.next_param, DATA_LENGTH);

    make_abi_u32(param, 9);
    feed_param(&ctx, param, 164);  // DATA_LENGTH
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);
    TEST_ASSERT_EQUAL(ctx.data_length, 9);
}

void test_safe_transfer_extra_params_tolerated(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_TRANSFER);
    run_safe_transfer(&ctx, 9);
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);
    // The payload words past NONE must not error
    uint8_t param[PARAMETER_LENGTH] = {0};
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 196;
    msg.result = ETH_PLUGIN_RESULT_SUCCESSFUL;
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_SUCCESSFUL);
}

void test_safe_transfer_data_length_misplaced_rejected(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_TRANSFER);
    uint8_t param[PARAMETER_LENGTH];
    make_abi_address(param, 0xAA);
    feed_param(&ctx, param, 4);
    feed_param(&ctx, param, 36);
    make_abi_u32(param, 1);
    feed_param(&ctx, param, 68);
    feed_param(&ctx, param, 100);
    make_abi_u32(param, 160);
    feed_param(&ctx, param, 132);
    // length word announced at 164, fed at 196
    make_abi_u32(param, 9);
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 196;
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Tests — PROVIDE_PARAMETER safeBatchTransferFrom
// =============================================================================
//
// ABI layout of safeBatchTransferFrom(from, to, ids[], values[], data):
//   head:
//     [0..31]      from
//     [32..63]     to
//     [64..95]     offset_to_ids (relative to start of args)
//     [96..127]    offset_to_values
//     [128..159]   offset_to_data
//   then at offset_to_ids: [length_ids:32] [ids[0]:32] ... [ids[N-1]:32]
//   then at offset_to_values: [length_values:32] [values[0]:32] ... [values[N-1]:32]
//
// `parameterOffset` exposed to the plugin starts at 4 (selector consumed)
// and points to the start of the param. The offset values stored in the
// head are then +4 by the parser to translate "from start of args" into
// "from start of calldata".

static void run_batch_head(erc1155_context_t *ctx,
                           uint32_t ids_offset_args,
                           uint32_t values_offset_args,
                           uint32_t data_offset_args) {
    uint8_t param[PARAMETER_LENGTH];
    // FROM
    make_abi_address(param, 0xAA);
    feed_param(ctx, param, 4);
    // TO
    make_abi_address(param, 0xBB);
    feed_param(ctx, param, 36);
    // IDS_OFFSET (in args-frame units, parser adds +4)
    make_abi_u32(param, ids_offset_args);
    feed_param(ctx, param, 68);
    // VALUES_OFFSET
    make_abi_u32(param, values_offset_args);
    feed_param(ctx, param, 100);
    // DATA_OFFSET
    make_abi_u32(param, data_offset_args);
    feed_param(ctx, param, 132);
}

void test_batch_empty_ids_skips_to_value_length(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_BATCH);
    run_batch_head(&ctx, 160, 192, 224);
    TEST_ASSERT_EQUAL(ctx.next_param, TOKEN_IDS_LENGTH);

    uint8_t param[PARAMETER_LENGTH];
    // ids array length = 0: must go to VALUE_LENGTH to avoid --ids_array_len
    // underflow (uint16_t wraps to 65535).
    make_abi_u32(param, 0);
    feed_param(&ctx, param, 164);
    TEST_ASSERT_EQUAL(ctx.next_param, VALUE_LENGTH);
    TEST_ASSERT_EQUAL(ctx.ids_array_len, 0);
}

void test_batch_two_pair_walk(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_BATCH);
    // ids[] right after the 5-word head; values[] at 160 + 32 + 2*32 = 256;
    // data at 256 + 32 + 2*32 = 352.
    run_batch_head(&ctx, 160, 256, 352);
    TEST_ASSERT_EQUAL(ctx.next_param, TOKEN_IDS_LENGTH);

    uint8_t param[PARAMETER_LENGTH];
    // ids array length = 2
    make_abi_u32(param, 2);
    feed_param(&ctx, param, 164);  // = ids_offset_args + 4
    TEST_ASSERT_EQUAL(ctx.next_param, TOKEN_ID);
    TEST_ASSERT_EQUAL(ctx.ids_array_len, 2);
    TEST_ASSERT_EQUAL(ctx.batch_displayed, 2);
    TEST_ASSERT_FALSE(ctx.batch_truncated);

    // ids[0] = 7
    make_abi_u32(param, 7);
    feed_param(&ctx, param, 196);
    // ids[1] = 9 → after this, next_param flips to VALUE_LENGTH
    make_abi_u32(param, 9);
    feed_param(&ctx, param, 228);
    TEST_ASSERT_EQUAL(ctx.next_param, VALUE_LENGTH);

    // values array length = 2
    make_abi_u32(param, 2);
    feed_param(&ctx, param, 260);  // = values_offset_args + 4
    TEST_ASSERT_EQUAL(ctx.next_param, VALUE);

    // values[0] = 100
    make_abi_u32(param, 100);
    feed_param(&ctx, param, 292);
    // values[1] = 50
    make_abi_u32(param, 50);
    feed_param(&ctx, param, 324);
    TEST_ASSERT_EQUAL(ctx.next_param, DATA_LENGTH);

    // data length = 9
    make_abi_u32(param, 9);
    feed_param(&ctx, param, 356);  // = data_offset_args + 4
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);
    TEST_ASSERT_EQUAL(ctx.data_length, 9);
    // total quantity = 100 + 50 = 150
    TEST_ASSERT_EQUAL(LOWER(LOWER(ctx.value)), 150);

    // batch_ids / batch_values captured the first two pairs.
    TEST_ASSERT_EQUAL(ctx.batch_displayed, 2);
    TEST_ASSERT_EQUAL(ctx.batch_ids[0][PARAMETER_LENGTH - 1], 7);
    TEST_ASSERT_EQUAL(ctx.batch_ids[1][PARAMETER_LENGTH - 1], 9);
    TEST_ASSERT_EQUAL(ctx.batch_values[0][PARAMETER_LENGTH - 1], 100);
    TEST_ASSERT_EQUAL(ctx.batch_values[1][PARAMETER_LENGTH - 1], 50);
}

void test_batch_truncates_beyond_display_max(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_BATCH);
    run_batch_head(&ctx, 160, 160 + 32 + 32 * 5, 160 + 2 * (32 + 32 * 5));

    uint8_t param[PARAMETER_LENGTH];
    // 5 ids -> > ERC1155_BATCH_DISPLAY_MAX (3) -> batch_truncated.
    make_abi_u32(param, 5);
    feed_param(&ctx, param, 164);
    TEST_ASSERT_EQUAL(ctx.batch_displayed, ERC1155_BATCH_DISPLAY_MAX);
    TEST_ASSERT_TRUE(ctx.batch_truncated);
}

void test_batch_offset_mismatch_rejected(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_BATCH);
    run_batch_head(&ctx, 160, 256, 352);

    // Feed the ids_length parameter at the WRONG offset (one off);
    // the parser must reject rather than misalign.
    uint8_t param[PARAMETER_LENGTH];
    make_abi_u32(param, 2);
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 999;  // far past ids_offset
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_batch_arrays_length_mismatch_rejected(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_BATCH);
    run_batch_head(&ctx, 160, 256, 384);
    uint8_t param[PARAMETER_LENGTH];

    // Declare 2 ids
    make_abi_u32(param, 2);
    feed_param(&ctx, param, 164);
    make_abi_u32(param, 1);
    feed_param(&ctx, param, 196);
    make_abi_u32(param, 2);
    feed_param(&ctx, param, 228);
    // ...then declare 3 values (mismatch).
    make_abi_u32(param, 3);
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 260;
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_batch_aggregate_quantity_overflow_rejected(void) {
    // Two values: 2^255 + 2^255 wraps uint256 → handler must reject so
    // the "total quantity" line shown to the user does not lie.
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_SAFE_BATCH);
    run_batch_head(&ctx, 160, 256, 352);

    uint8_t param[PARAMETER_LENGTH];
    // ids array length = 2 + ids[0]=1, ids[1]=2
    make_abi_u32(param, 2);
    feed_param(&ctx, param, 164);
    make_abi_u32(param, 1);
    feed_param(&ctx, param, 196);
    make_abi_u32(param, 2);
    feed_param(&ctx, param, 228);
    // values array length = 2
    make_abi_u32(param, 2);
    feed_param(&ctx, param, 260);
    // values[0] = 2^255
    memset(param, 0, sizeof(param));
    param[0] = 0x80;
    feed_param(&ctx, param, 292);
    // values[1] = 2^255 — sum overflows uint256.
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    msg.parameterOffset = 324;
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Tests — PROVIDE_PARAMETER setApprovalForAll
// =============================================================================

void test_approval_for_all_walk(void) {
    erc1155_context_t ctx = {0};
    run_init(&ctx, SEL_APPROVE_FOR_ALL);

    uint8_t param[PARAMETER_LENGTH];
    make_abi_address(param, 0xAA);
    feed_param(&ctx, param, 0);  // OPERATOR
    TEST_ASSERT_EQUAL(ctx.next_param, APPROVED);

    memset(param, 0, sizeof(param));
    param[PARAMETER_LENGTH - 1] = 1;
    feed_param(&ctx, param, 0);  // APPROVED = true
    TEST_ASSERT_EQUAL(ctx.next_param, NONE);
    TEST_ASSERT_TRUE(ctx.approved);

    // Extra param after NONE → error.
    ethPluginProvideParameter_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.parameter = param;
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// =============================================================================
// Tests — FINALIZE
// =============================================================================

void test_finalize_safe_transfer_six_screens(void) {
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(msg.numScreens, 6);
}

void test_finalize_batch_screens_formula(void) {
    // 5 fixed + 2 * batch_displayed + 1 if truncated.
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER,
                             .batch_displayed = ERC1155_BATCH_DISPLAY_MAX,
                             .batch_truncated = true};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.numScreens, 5 + 2 * ERC1155_BATCH_DISPLAY_MAX + 1);
}

void test_finalize_transfer_with_data_extra_screen(void) {
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER, .data_length = 9};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL(msg.numScreens, 7);

    erc1155_context_t batch = {.selectorIndex = SAFE_BATCH_TRANSFER,
                               .batch_displayed = 2,
                               .data_length = 9};
    msg.pluginContext = (uint8_t *) &batch;
    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.numScreens, 5 + 2 * 2 + 1);
}

void test_finalize_approval_three_screens(void) {
    erc1155_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.numScreens, 3);
}

void test_finalize_eth_attached_rejected(void) {
    // None of the three ERC-1155 selectors are payable. Any non-zero
    // ETH value attached to the tx means the calldata is hostile.
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    txContent_t tx = {0};
    tx.value.value[0] = 0x01;
    tx.value.length = 1;
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_finalize_unknown_selector_rejects(void) {
    erc1155_context_t ctx = {.selectorIndex = 0x7F};
    txContent_t tx = {0};
    ethPluginFinalize_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.txContent = &tx;
    erc1155_plugin_call(ETH_PLUGIN_FINALIZE, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_query_id_unknown_selector_rejects(void) {
    erc1155_context_t ctx = {.selectorIndex = 0x7F};
    char name[32] = {0};
    char version[16] = {0};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_provide_info_returns_ok(void) {
    // handle_provide_info_1155 just sets result = OK; pin the dispatch
    // path through erc1155_plugin_call too.
    ethPluginProvideInfo_t msg = {0};
    erc1155_plugin_call(ETH_PLUGIN_PROVIDE_INFO, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
}

// =============================================================================
// Tests — QUERY_CONTRACT_ID
// =============================================================================

void test_query_contract_id_per_selector(void) {
    char name[32], version[16];

    erc1155_context_t ctx_approve = {.selectorIndex = SET_APPROVAL_FOR_ALL};
    ethQueryContractID_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx_approve;
    msg.name = name;
    msg.nameLength = sizeof(name);
    msg.version = version;
    msg.versionLength = sizeof(version);
    memset(name, 0, sizeof(name));
    memset(version, 0, sizeof(version));
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL_STRING(name, "NFT allowance");
    TEST_ASSERT_EQUAL_STRING(version, "manage");

    erc1155_context_t ctx_transfer = {.selectorIndex = SAFE_TRANSFER};
    msg.pluginContext = (uint8_t *) &ctx_transfer;
    memset(name, 0, sizeof(name));
    memset(version, 0, sizeof(version));
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL_STRING(name, "NFT");
    TEST_ASSERT_EQUAL_STRING(version, "Transfer");

    erc1155_context_t ctx_batch = {.selectorIndex = SAFE_BATCH_TRANSFER};
    msg.pluginContext = (uint8_t *) &ctx_batch;
    memset(name, 0, sizeof(name));
    memset(version, 0, sizeof(version));
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, &msg);
    TEST_ASSERT_EQUAL_STRING(name, "NFT");
    TEST_ASSERT_EQUAL_STRING(version, "Batch Transfer");
}

// =============================================================================
// Tests — QUERY_CONTRACT_UI (spot checks)
// =============================================================================

static union extraInfo_t g_nft_info;

static void prep_nft_info(void) {
    memset(&g_nft_info, 0, sizeof(g_nft_info));
    strncpy(g_nft_info.nft.collectionName, "Wares", sizeof(g_nft_info.nft.collectionName) - 1);
    memset(g_nft_info.nft.contractAddress, 0xBB, ADDRESS_LENGTH);
}

void test_ui_null_item1_rejected(void) {
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.item1 = NULL;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

void test_ui_safe_transfer_screen0(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.item1 = &g_nft_info;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 0;
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Owner");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_safe_transfer_screen1_to(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.item1 = &g_nft_info;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 1;
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "To");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_batch_total_quantity_screen(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER, .array_index = 2};
    LOWER(LOWER(ctx.value)) = 150;
    char title[32], body[64];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.item1 = &g_nft_info;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    msg.screenIndex = 4;  // BATCH_SCREEN_TOTAL_QUANTITY
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Total Quantity");
    TEST_ASSERT_EQUAL_STRING(body, "150 from 2 NFT IDs");
}

void test_ui_batch_truncation_warning_screen(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER,
                             .batch_displayed = ERC1155_BATCH_DISPLAY_MAX,
                             .batch_truncated = true,
                             .array_index = 7};
    char title[32], body[64];
    ethQueryContractUI_t msg = {0};
    msg.pluginContext = (uint8_t *) &ctx;
    msg.item1 = &g_nft_info;
    msg.title = title;
    msg.titleLength = sizeof(title);
    msg.msg = body;
    msg.msgLength = sizeof(body);
    // Warning sits after the 5 fixed screens + 2*display pair screens.
    msg.screenIndex = (uint8_t) (5 + 2 * ERC1155_BATCH_DISPLAY_MAX);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "WARNING");
    TEST_ASSERT_EQUAL_STRING(body, "Only first 3 of 7 IDs shown");
}

// =============================================================================
// Remaining sub-screens for each selector
// =============================================================================

static void prep_ui_msg(ethQueryContractUI_t *msg,
                        erc1155_context_t *ctx,
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

// SAFE_TRANSFER sub-screens

void test_ui_safe_transfer_screen2_collection(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 2);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Collection Name");
    TEST_ASSERT_EQUAL_STRING(body, "Wares");
}

void test_ui_safe_transfer_screen3_nft_address(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 3);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Address");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_safe_transfer_screen4_nft_id(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    ctx.tokenId[31] = 99;
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 4);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT ID");
    TEST_ASSERT_EQUAL_STRING(body, "99");
}

void test_ui_safe_transfer_screen5_quantity(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    LOWER(LOWER(ctx.value)) = 1234;
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 5);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Quantity");
    TEST_ASSERT_EQUAL_STRING(body, "1234");
}

void test_ui_safe_transfer_screen6_data(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER, .data_length = 9};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 6);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "Transfer With Data");
    TEST_ASSERT_EQUAL_STRING(body, "9 bytes");
}

void test_ui_batch_data_screen_after_warning(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER,
                             .batch_displayed = ERC1155_BATCH_DISPLAY_MAX,
                             .batch_truncated = true,
                             .data_length = 9};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, (uint8_t) (5 + 2 * ERC1155_BATCH_DISPLAY_MAX + 1));
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_OK);
    TEST_ASSERT_EQUAL_STRING(title, "Transfer With Data");
    TEST_ASSERT_EQUAL_STRING(body, "9 bytes");
}

void test_ui_safe_transfer_unknown_screen_rejected(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 99);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// SET_APPROVAL_FOR_ALL sub-screens

void test_ui_approval_for_all_screen0_allow(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 0);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Allow");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_approval_for_all_screen0_revoke(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = false};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 0);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Revoke");
}

void test_ui_approval_for_all_screen1_to_manage_all(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 1);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "To Manage ALL");
    TEST_ASSERT_EQUAL_STRING(body, "Wares");
}

void test_ui_approval_for_all_screen2_nft_address(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 2);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Address");
    TEST_ASSERT_EQUAL_STRING(body, "0xADDR");
}

void test_ui_approval_for_all_unknown_screen_rejected(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SET_APPROVAL_FOR_ALL, .approved = true};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 99);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL(msg.result, ETH_PLUGIN_RESULT_ERROR);
}

// SAFE_BATCH_TRANSFER sub-screens

void test_ui_batch_screen0_owner(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 0);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Owner");
}

void test_ui_batch_screen1_to(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 1);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "To");
}

void test_ui_batch_screen2_collection(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 2);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Collection Name");
    TEST_ASSERT_EQUAL_STRING(body, "Wares");
}

void test_ui_batch_screen3_nft_address(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 3);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT Address");
}

void test_ui_batch_pair_nft_id_screen(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER, .batch_displayed = 2};
    ctx.batch_ids[0][31] = 7;
    char title[32], body[64];
    ethQueryContractUI_t msg;
    // Pair base is at index 5. Even offset = NFT ID, odd = Quantity.
    prep_ui_msg(&msg, &ctx, title, body, 5);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "NFT ID #1");
    TEST_ASSERT_EQUAL_STRING(body, "7");
}

void test_ui_batch_pair_quantity_screen(void) {
    prep_nft_info();
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER, .batch_displayed = 2};
    ctx.batch_values[0][31] = 42;
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 6);  // Pair 0, value half
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
    TEST_ASSERT_EQUAL_STRING(title, "Quantity #1");
    TEST_ASSERT_EQUAL_STRING(body, "42");
}

void test_ui_batch_unsupported_screen_rejected(void) {
    prep_nft_info();
    // batch_displayed = 1 so the pair range is [5..6]; index 99 falls
    // past the gap and must error.
    erc1155_context_t ctx = {.selectorIndex = SAFE_BATCH_TRANSFER,
                             .batch_displayed = 1,
                             .batch_truncated = false};
    char title[32], body[64];
    ethQueryContractUI_t msg;
    prep_ui_msg(&msg, &ctx, title, body, 99);
    erc1155_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, &msg);
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
    RUN_TEST(test_init_recognises_three_selectors);
    RUN_TEST(test_init_unknown_selector_falls_back);
    RUN_TEST(test_init_zeroes_stale_context);
    RUN_TEST(test_batch_empty_ids_skips_to_value_length);
    RUN_TEST(test_safe_transfer_walk);
    RUN_TEST(test_safe_transfer_extra_params_tolerated);
    RUN_TEST(test_safe_transfer_data_length_misplaced_rejected);
    RUN_TEST(test_batch_two_pair_walk);
    RUN_TEST(test_batch_truncates_beyond_display_max);
    RUN_TEST(test_batch_offset_mismatch_rejected);
    RUN_TEST(test_batch_arrays_length_mismatch_rejected);
    RUN_TEST(test_batch_aggregate_quantity_overflow_rejected);
    RUN_TEST(test_approval_for_all_walk);
    RUN_TEST(test_finalize_safe_transfer_six_screens);
    RUN_TEST(test_finalize_batch_screens_formula);
    RUN_TEST(test_finalize_transfer_with_data_extra_screen);
    RUN_TEST(test_finalize_approval_three_screens);
    RUN_TEST(test_finalize_eth_attached_rejected);
    RUN_TEST(test_finalize_unknown_selector_rejects);
    RUN_TEST(test_query_id_unknown_selector_rejects);
    RUN_TEST(test_provide_info_returns_ok);
    RUN_TEST(test_query_contract_id_per_selector);
    RUN_TEST(test_ui_null_item1_rejected);
    RUN_TEST(test_ui_safe_transfer_screen0);
    RUN_TEST(test_ui_safe_transfer_screen1_to);
    RUN_TEST(test_ui_batch_total_quantity_screen);
    RUN_TEST(test_ui_batch_truncation_warning_screen);
    RUN_TEST(test_ui_safe_transfer_screen2_collection);
    RUN_TEST(test_ui_safe_transfer_screen3_nft_address);
    RUN_TEST(test_ui_safe_transfer_screen4_nft_id);
    RUN_TEST(test_ui_safe_transfer_screen5_quantity);
    RUN_TEST(test_ui_safe_transfer_screen6_data);
    RUN_TEST(test_ui_batch_data_screen_after_warning);
    RUN_TEST(test_ui_safe_transfer_unknown_screen_rejected);
    RUN_TEST(test_ui_approval_for_all_screen0_allow);
    RUN_TEST(test_ui_approval_for_all_screen0_revoke);
    RUN_TEST(test_ui_approval_for_all_screen1_to_manage_all);
    RUN_TEST(test_ui_approval_for_all_screen2_nft_address);
    RUN_TEST(test_ui_approval_for_all_unknown_screen_rejected);
    RUN_TEST(test_ui_batch_screen0_owner);
    RUN_TEST(test_ui_batch_screen1_to);
    RUN_TEST(test_ui_batch_screen2_collection);
    RUN_TEST(test_ui_batch_screen3_nft_address);
    RUN_TEST(test_ui_batch_pair_nft_id_screen);
    RUN_TEST(test_ui_batch_pair_quantity_screen);
    RUN_TEST(test_ui_batch_unsupported_screen_rejected);
    return UNITY_END();
}
