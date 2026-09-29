/**
 * @file test_manage_asset_info.c
 * @brief Unit tests for get_matching_asset_info / forget_known_assets at
 *        src/manage_asset_info.c.
 *
 * manage_asset_info is the unified front for the legacy token-OR-NFT
 * registry pair. The Ethereum app keeps ERC-20 token info and ERC-721/1155
 * NFT info in two distinct stores; callers that don't care which one (e.g.
 * the GCS dispatcher when resolving a contract for display) ask through
 * this single helper. Tokens win over NFTs on shared addresses (the
 * comment in the source pins that precedence).
 *
 * Pin both branches of the lookup plus the bundled forget.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "manage_asset_info.h"
#include "token_info.h"
#include "nft_info.h"

// =============================================================================
// Wraps
// =============================================================================

static s_token_info s_token_fake;
static s_nft_info s_nft_fake;

static const s_token_info *g_token_ret = NULL;
static const s_nft_info *g_nft_ret = NULL;

const s_token_info *get_matching_token_info(const uint64_t *chain_id, const uint8_t *address) {
    (void) chain_id;
    (void) address;
    return g_token_ret;
}

const s_nft_info *get_matching_nft_info(const uint64_t *chain_id, const uint8_t *address) {
    (void) chain_id;
    (void) address;
    return g_nft_ret;
}

static int g_clear_token_calls = 0;
void clear_token_infos(void) {
    g_clear_token_calls++;
}

static int g_clear_nft_calls = 0;
void clear_nft_infos(void) {
    g_clear_nft_calls++;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    g_clear_token_calls = 0;
    g_clear_nft_calls = 0;
    g_token_ret = NULL;
    g_nft_ret = NULL;
    pluginType = PLUGIN_TYPE_EXTERNAL;
}

// =============================================================================
// get_matching_asset_info -- token wins over NFT
// =============================================================================

void test_token_match_returns_token_skips_nft(void) {
    reset();
    uint64_t cid = 1;
    uint8_t addr[20] = {0};
    g_token_ret = &s_token_fake;
    // get_matching_nft_info MUST NOT be reached when the token lookup hits.
    extraInfo_t *got = get_matching_asset_info(&cid, addr);
    TEST_ASSERT_EQUAL_PTR(got, (extraInfo_t *) &s_token_fake);
}

void test_no_token_falls_back_to_nft(void) {
    reset();
    uint64_t cid = 1;
    uint8_t addr[20] = {0};
    g_token_ret = NULL;
    g_nft_ret = &s_nft_fake;
    extraInfo_t *got = get_matching_asset_info(&cid, addr);
    TEST_ASSERT_EQUAL_PTR(got, (extraInfo_t *) &s_nft_fake);
}

void test_no_token_no_nft_returns_null(void) {
    reset();
    uint64_t cid = 1;
    uint8_t addr[20] = {0};
    g_token_ret = NULL;
    g_nft_ret = NULL;
    TEST_ASSERT_NULL(get_matching_asset_info(&cid, addr));
}

void test_no_reading_plugin_returns_null(void) {
    const pluginType_t types[] = {PLUGIN_TYPE_NONE, PLUGIN_TYPE_SWAP_WITH_CALLDATA};
    uint64_t cid = 1;
    uint8_t addr[20] = {0};
    g_token_ret = &s_token_fake;
    g_nft_ret = &s_nft_fake;
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        pluginType = types[i];
        TEST_ASSERT_NULL(get_matching_asset_info(&cid, addr));
    }
}

// =============================================================================
// forget_known_assets -- clears both registries
// =============================================================================

void test_forget_known_assets_clears_both(void) {
    forget_known_assets();
    TEST_ASSERT_EQUAL(g_clear_token_calls, 1);
    TEST_ASSERT_EQUAL(g_clear_nft_calls, 1);
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_token_match_returns_token_skips_nft);
    RUN_TEST(test_no_token_falls_back_to_nft);
    RUN_TEST(test_no_token_no_nft_returns_null);
    RUN_TEST(test_no_reading_plugin_returns_null);
    RUN_TEST(test_forget_known_assets_clears_both);
    return UNITY_END();
}
