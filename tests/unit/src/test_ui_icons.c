/**
 * @file test_ui_icons.c
 * @brief Unit tests for get_app_icon / get_home_icon / get_tx_icon at
 *        src/nbgl/ui_icons.c.
 *
 * The icon-routing layer decides which logo flashes on the review /
 * home / transaction screens. The choice depends on:
 *   - whether a caller app is hosting Ethereum (plugin or clone)
 *   - whether the caller app shipped its own icon
 *   - the current pluginType (PLUGIN_TYPE_EXTERNAL vs internal)
 *   - the current chain (Mainnet vs other)
 *
 * A misroute is a visual spoof vector (user sees the wrong brand mark
 * on a confirmation screen). Pin every branch.
 *
 *   get_app_icon:
 *     - caller_icon=false                   -> app ICONGLYPH
 *     - caller_icon=true, g_caller_app=NULL -> app ICONGLYPH
 *     - caller_icon=true, icon=NULL         -> app ICONGLYPH (PRINTF fallback)
 *     - caller_icon=true, icon=X            -> X
 *
 *   get_home_icon:
 *     - g_caller_app=NULL                   -> app ICONHOME
 *     - g_caller_app->icon=NULL             -> app ICONHOME
 *     - g_caller_app->icon=X                -> X
 *
 *   get_tx_icon:
 *     - fromPlugin=true, EXTERNAL, name matches toAddress -> caller icon
 *     - fromPlugin=true, EXTERNAL, name doesn't match     -> NULL
 *     - fromPlugin=false, g_caller_app != NULL (clone)    -> caller icon
 *     - fromPlugin=false, NULL caller, chain == config    -> app ICONGLYPH
 *     - fromPlugin=false, NULL caller, chain != config    -> network icon
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "caller_app.h"
#include "nbgl_types.h"
#include "Mocknetwork.h"  // find_dynamic_network_by_chain_id
#include "ui_icons.h"
#include "wraps.h"

// =============================================================================
// Storage for ICONGLYPH / ICONHOME (referenced as `test_glyph` / `test_home_glyph`
// via the DEFS line). glyphs.h declares them extern; we provide the storage.
// =============================================================================

nbgl_icon_details_t test_glyph = {.width = 64};
nbgl_icon_details_t test_home_glyph = {.width = 96};

// Caller's own icon used in several tests.
static nbgl_icon_details_t s_caller_icon = {.width = 128};

// =============================================================================
// Wraps
// =============================================================================

// get_network_icon_from_chain_id used to live in network_icons.c and was
// linker-wrappable from this test target. After the upstream merge it
// is now in the same TU as get_tx_icon -- --wrap doesn't intercept
// intra-TU calls, so we drive the real impl through its dependency:
// find_dynamic_network_by_chain_id. Tests that want the icon-found
// branch point g_dyn_net_ret at a network with a non-NULL bitmap;
// tests that want the not-found branch leave it at NULL.
static network_info_t *g_dyn_net_ret = NULL;
static network_info_t *find_dynamic_network_by_chain_id_stub(uint64_t chain_id,
                                                             int cmock_num_calls) {
    (void) chain_id;
    (void) cmock_num_calls;
    return g_dyn_net_ret;
}

static uint64_t s_tx_chain_id = 1;
static uint64_t get_tx_chain_id_stub(int cmock_num_calls) {
    (void) cmock_num_calls;
    return s_tx_chain_id;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    g_caller_app = NULL;
    pluginType = PLUGIN_TYPE_OLD_INTERNAL;
    memset(&strings, 0, sizeof(strings));
    // g_chain_config points to g_chainConfig (in app_globals.c) with
    // chain_id == 1. Tests that need a mismatch flip s_tx_chain_id.
    g_chainConfig.chain_id = 1;
    s_tx_chain_id = 1;
    g_dyn_net_ret = NULL;
}

// =============================================================================
// get_app_icon
// =============================================================================

void test_app_icon_no_caller_icon_returns_iconglyph(void) {
    TEST_ASSERT_EQUAL_PTR(get_app_icon(false), &test_glyph);
}

void test_app_icon_caller_requested_null_caller_returns_iconglyph(void) {
    g_caller_app = NULL;
    TEST_ASSERT_EQUAL_PTR(get_app_icon(true), &test_glyph);
}

void test_app_icon_caller_has_no_icon_returns_iconglyph(void) {
    caller_app_t caller = {.name = "Plugin", .icon = NULL};
    g_caller_app = &caller;
    TEST_ASSERT_EQUAL_PTR(get_app_icon(true), &test_glyph);
}

void test_app_icon_caller_has_icon_returns_caller_icon(void) {
    caller_app_t caller = {.name = "Plugin", .icon = &s_caller_icon};
    g_caller_app = &caller;
    TEST_ASSERT_EQUAL_PTR(get_app_icon(true), &s_caller_icon);
}

// =============================================================================
// get_home_icon
// =============================================================================

void test_home_icon_null_caller_returns_iconhome(void) {
    g_caller_app = NULL;
    TEST_ASSERT_EQUAL_PTR(get_home_icon(), &test_home_glyph);
}

void test_home_icon_caller_without_icon_returns_iconhome(void) {
    caller_app_t caller = {.icon = NULL};
    g_caller_app = &caller;
    TEST_ASSERT_EQUAL_PTR(get_home_icon(), &test_home_glyph);
}

void test_home_icon_caller_with_icon_returns_caller_icon(void) {
    caller_app_t caller = {.icon = &s_caller_icon};
    g_caller_app = &caller;
    TEST_ASSERT_EQUAL_PTR(get_home_icon(), &s_caller_icon);
}

// =============================================================================
// get_tx_icon
// =============================================================================

void test_tx_icon_plugin_external_matching_name_returns_caller_icon(void) {
    caller_app_t caller = {.name = "MyPlugin", .icon = &s_caller_icon};
    g_caller_app = &caller;
    pluginType = PLUGIN_TYPE_EXTERNAL;
    strlcpy(strings.common.toAddress, "MyPlugin", sizeof(strings.common.toAddress));
    TEST_ASSERT_EQUAL_PTR(get_tx_icon(true), &s_caller_icon);
}

void test_tx_icon_plugin_external_no_name_match_returns_null(void) {
    caller_app_t caller = {.name = "MyPlugin", .icon = &s_caller_icon};
    g_caller_app = &caller;
    pluginType = PLUGIN_TYPE_EXTERNAL;
    strlcpy(strings.common.toAddress, "SomethingElse", sizeof(strings.common.toAddress));
    TEST_ASSERT_NULL(get_tx_icon(true));
}

void test_tx_icon_clone_returns_caller_icon(void) {
    // fromPlugin=false but caller present == clone case (Ethereum running
    // under a clone app). Use the caller's icon.
    caller_app_t caller = {.icon = &s_caller_icon};
    g_caller_app = &caller;
    TEST_ASSERT_EQUAL_PTR(get_tx_icon(false), &s_caller_icon);
}

void test_tx_icon_standard_matching_chain_returns_app_iconglyph(void) {
    g_caller_app = NULL;
    s_tx_chain_id = 1;
    g_chainConfig.chain_id = 1;
    TEST_ASSERT_EQUAL_PTR(get_tx_icon(false), &test_glyph);
}

void test_tx_icon_standard_other_chain_returns_network_icon(void) {
    // No caller, chain mismatch -> get_tx_icon delegates to
    // get_network_icon_from_chain_id. Drive the latter through its
    // dynamic-network lookup: hand back a network with a non-NULL
    // bitmap so the helper returns &net.icon.
    g_caller_app = NULL;
    s_tx_chain_id = 137;
    g_chainConfig.chain_id = 1;
    static uint8_t s_bitmap_bytes[8];
    static network_info_t s_net;
    memset(&s_net, 0, sizeof(s_net));
    s_net.icon.bitmap = s_bitmap_bytes;
    s_net.icon.width = 32;
    g_dyn_net_ret = &s_net;
    TEST_ASSERT_EQUAL_PTR(get_tx_icon(false), &s_net.icon);
}

void setUp(void) {
    Mocknetwork_Init();
    get_tx_chain_id_StubWithCallback(get_tx_chain_id_stub);
    find_dynamic_network_by_chain_id_StubWithCallback(find_dynamic_network_by_chain_id_stub);
    reset();
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_app_icon_no_caller_icon_returns_iconglyph);
    RUN_TEST(test_app_icon_caller_requested_null_caller_returns_iconglyph);
    RUN_TEST(test_app_icon_caller_has_no_icon_returns_iconglyph);
    RUN_TEST(test_app_icon_caller_has_icon_returns_caller_icon);
    RUN_TEST(test_home_icon_null_caller_returns_iconhome);
    RUN_TEST(test_home_icon_caller_without_icon_returns_iconhome);
    RUN_TEST(test_home_icon_caller_with_icon_returns_caller_icon);
    RUN_TEST(test_tx_icon_plugin_external_matching_name_returns_caller_icon);
    RUN_TEST(test_tx_icon_plugin_external_no_name_match_returns_null);
    RUN_TEST(test_tx_icon_clone_returns_caller_icon);
    RUN_TEST(test_tx_icon_standard_matching_chain_returns_app_iconglyph);
    RUN_TEST(test_tx_icon_standard_other_chain_returns_network_icon);
    return UNITY_END();
}
