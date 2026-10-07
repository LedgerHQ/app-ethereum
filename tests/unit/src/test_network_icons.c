/**
 * @file test_network_icons.c
 * @brief Unit tests for get_network_icon_from_chain_id / get_clone_network_icon
 *        at src/nbgl/ui_icons.c (formerly in src/nbgl/network_icons.c
 *        before the upstream merge of the two TUs).
 *
 * The icon lookup decides which logo flashes on the review screen for a
 * given chain. The user reads "Polygon" because the icon matches Polygon's
 * brand mark, not because the underlying chain_id is 137 -- so a mis-routed
 * icon is a visual spoof vector.
 *
 * Pin the three resolution branches (Nano path, !SCREEN_SIZE_WALLET):
 *
 *  - dynamic network registered + has bitmap -> return its icon
 *  - dynamic network missing, chain_id == 1 -> fallback to app ICONGLYPH
 *  - dynamic network missing, chain_id != 1 -> NULL
 *  - dynamic network registered but bitmap == NULL -> fallback path
 *
 * Plus get_clone_network_icon's three branches:
 *  - NULL caller_app             NULL
 *  - PLUGIN type caller_app      NULL
 *  - CLONE type caller_app       return icon
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "nbgl_types.h"
#include "caller_app.h"
#include "network.h"
#include "ui_icons.h"  // get_network_icon_from_chain_id / get_clone_network_icon
#include "chain_config.h"

// =============================================================================
// Storage for the macro icons referenced by ui_icons.c. ICONGLYPH and
// ICONHOME are #defines that resolve to C_chain_N_*px symbols in
// production; in test we redirect them via DEFS to test_glyph /
// test_home_glyph and provide storage here. test_home_glyph is only
// referenced by get_home_icon, which this suite doesn't exercise, but
// the linker still needs the symbol to satisfy ui_icons.c.
// =============================================================================

nbgl_icon_details_t test_glyph = {.width = 64, .height = 64};
nbgl_icon_details_t test_home_glyph = {.width = 96, .height = 96};

// =============================================================================
// Wraps
// =============================================================================

static network_info_t *g_dyn_net_ret = NULL;
network_info_t *find_dynamic_network_by_chain_id(uint64_t chain_id) {
    (void) chain_id;
    return g_dyn_net_ret;
}

// =============================================================================
// Fixture
// =============================================================================

static network_info_t s_net;
static uint8_t s_bitmap_data[32];

static void reset(void) {
    memset(&s_net, 0, sizeof(s_net));
    g_dyn_net_ret = NULL;
}

// =============================================================================
// get_network_icon_from_chain_id
// =============================================================================

void test_dynamic_network_with_bitmap_returns_its_icon(void) {
    s_net.icon.bitmap = s_bitmap_data;
    s_net.icon.width = 32;
    g_dyn_net_ret = &s_net;
    uint64_t cid = 1234;
    const nbgl_icon_details_t *icon = get_network_icon_from_chain_id(&cid);
    TEST_ASSERT_EQUAL_PTR(icon, &s_net.icon);
}

void test_dynamic_network_with_null_bitmap_falls_back(void) {
    // bitmap NULL: skip dynamic entry, fall through to hardcoded path.
    s_net.icon.bitmap = NULL;
    g_dyn_net_ret = &s_net;
    uint64_t cid = 1;  // mainnet
    const nbgl_icon_details_t *icon = get_network_icon_from_chain_id(&cid);
#ifdef SCREEN_SIZE_WALLET
    // Wallet devices scan g_network_icons[]; stub has no mainnet entry.
    TEST_ASSERT_NULL(icon);
#else
    // Nano: mainnet falls back to ICONGLYPH.
    TEST_ASSERT_EQUAL_PTR(icon, &test_glyph);
#endif
}

void test_no_dynamic_match_mainnet_falls_back_to_iconglyph(void) {
    g_dyn_net_ret = NULL;
    uint64_t cid = ETHEREUM_MAINNET_CHAINID;
    const nbgl_icon_details_t *icon = get_network_icon_from_chain_id(&cid);
#ifdef SCREEN_SIZE_WALLET
    // Wallet devices scan g_network_icons[]; stub has no mainnet entry.
    TEST_ASSERT_NULL(icon);
#else
    // Nano: mainnet special case → ICONGLYPH.
    TEST_ASSERT_EQUAL_PTR(icon, &test_glyph);
#endif
}

void test_no_dynamic_match_non_mainnet_returns_null(void) {
    g_dyn_net_ret = NULL;
    uint64_t cid = 137;  // Polygon, not mainnet
    TEST_ASSERT_NULL(get_network_icon_from_chain_id(&cid));
}

// =============================================================================
// get_clone_network_icon
// =============================================================================

void test_clone_icon_null_caller_returns_null(void) {
    TEST_ASSERT_NULL(get_clone_network_icon(NULL));
}

void test_clone_icon_plugin_type_returns_null(void) {
    caller_app_t caller = {.type = CALLER_TYPE_PLUGIN, .icon = &test_glyph};
    TEST_ASSERT_NULL(get_clone_network_icon(&caller));
}

void test_clone_icon_clone_type_returns_icon(void) {
    caller_app_t caller = {.type = CALLER_TYPE_CLONE, .icon = &test_glyph};
    TEST_ASSERT_EQUAL_PTR(get_clone_network_icon(&caller), &test_glyph);
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dynamic_network_with_bitmap_returns_its_icon);
    RUN_TEST(test_dynamic_network_with_null_bitmap_falls_back);
    RUN_TEST(test_no_dynamic_match_mainnet_falls_back_to_iconglyph);
    RUN_TEST(test_no_dynamic_match_non_mainnet_returns_null);
    RUN_TEST(test_clone_icon_null_caller_returns_null);
    RUN_TEST(test_clone_icon_plugin_type_returns_null);
    RUN_TEST(test_clone_icon_clone_type_returns_icon);
    return UNITY_END();
}
