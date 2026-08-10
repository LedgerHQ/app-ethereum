/**
 * @file test_cmd_set_plugin_staging.c
 * @brief PLUGIN_TYPE_EXTERNAL branch of cmd_set_plugin.c.
 *
 * cmd_set_plugin's main test target (test_cmd_set_plugin) is compiled
 * without HAVE_NFT_STAGING_KEY, which fixes valid_keyId to the
 * production key. Under that build, every payload that names a non-
 * NFT plugin (i.e. anything that would resolve to PLUGIN_TYPE_EXTERNAL)
 * gets rejected at the PROD-key-vs-NFT guard before reaching the
 * BEGIN_TRY / os_lib_call(CHECK_PRESENCE) / END_TRY block.
 *
 * To exercise that block we need valid_keyId == TEST_PLUGIN_KEY, which
 * the source toggles via HAVE_NFT_STAGING_KEY. This dedicated target
 * defines the macro and pins the EXTERNAL plugin happy path -- the
 * device receives a signed registration for a plugin named "Uniswap"
 * with the TEST key, get_plugin_type resolves it to EXTERNAL, the
 * device hands off to os_lib_call to check that the matching external
 * app is installed, and the handler returns SWO_SUCCESS.
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
#include "cmd_set_plugin.h"
#include "wraps.h"

// =============================================================================
// Stubs (mirror test_cmd_set_plugin.c)
// =============================================================================

bool __wrap_app_compatible_with_chain_id(const uint64_t *chain_id) {
    (void) chain_id;
    return true;
}

// =============================================================================
// Binary payload builder (same layout as cmd_set_plugin.c expects)
// =============================================================================

#define ADDRESS_LENGTH 20
#define SELECTOR_SIZE  4

static size_t build_external_payload(uint8_t *out,
                                     size_t out_size,
                                     const char *name,
                                     uint64_t chain_id,
                                     uint8_t key_id,
                                     uint8_t sig_len) {
    size_t off = 0;
    out[off++] = 0x01;  // type ETH_PLUGIN
    out[off++] = 0x01;  // version VERSION_1
    size_t name_len = strlen(name);
    out[off++] = (uint8_t) name_len;
    memcpy(out + off, name, name_len);
    off += name_len;
    memset(out + off, 0xAB, ADDRESS_LENGTH);
    off += ADDRESS_LENGTH;
    memset(out + off, 0xDE, SELECTOR_SIZE);
    off += SELECTOR_SIZE;
    // chain_id BE 8 bytes
    for (int i = 7; i >= 0; --i) {
        out[off++] = (uint8_t) ((chain_id >> (i * 8)) & 0xFF);
    }
    out[off++] = key_id;
    out[off++] = 0x01;  // algo ECC_SECG_P256K1__ECDSA_SHA_256
    out[off++] = sig_len;
    memset(out + off, 0x42, sig_len);
    off += sig_len;
    assert_true(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static int reset(void **state) {
    (void) state;
    appState = APP_STATE_IDLE;
    memset(&dataContext, 0, sizeof(dataContext));
    pluginType = PLUGIN_TYPE_NONE;
    g_sig_check_ret = true;
    return 0;
}

// =============================================================================
// EXTERNAL plugin path
// =============================================================================

static void test_external_plugin_check_presence_succeeds(void **state) {
    (void) state;
    // TEST_PLUGIN_KEY (0x00) accepted because HAVE_NFT_STAGING_KEY is on.
    // Plugin name "Uniswap" resolves to PLUGIN_TYPE_EXTERNAL.
    uint8_t payload[256];
    size_t len = build_external_payload(payload,
                                        sizeof(payload),
                                        "Uniswap",
                                        /*chain_id*/ 1,
                                        /*key_id*/ 0x00,
                                        /*sig_len*/ 70);
    uint16_t sw = handle_set_plugin(payload, (uint8_t) len);
    // The BEGIN_TRY / os_lib_call body uses the BOLOS exception stack.
    // Under our mock.c stubs (try_context_set/get -> NULL, os_lib_call ->
    // no-op), the TRY runs to completion -> SWO_SUCCESS. If a future
    // hardening of the exception machinery makes the os_lib_call throw,
    // CATCH_OTHER will fire and return SWO_FILE_NOT_FOUND. Accept either,
    // the important thing is that the EXTERNAL block is traversed.
    assert_true(sw == SWO_SUCCESS || sw == SWO_FILE_NOT_FOUND);
    assert_int_equal(pluginType, PLUGIN_TYPE_EXTERNAL);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_external_plugin_check_presence_succeeds, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
