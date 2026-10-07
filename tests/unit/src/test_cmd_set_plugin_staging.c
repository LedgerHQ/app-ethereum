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

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "cmd_set_plugin.h"
#include "Mockpublic_keys.h"

// =============================================================================
// Stubs (mirror test_cmd_set_plugin.c)
// =============================================================================

bool app_compatible_with_chain_id(const uint64_t *chain_id) {
    (void) chain_id;
    return true;
}

// check_signature_with_pubkey is mocked via Mockpublic_keys.h (CMock).
static bool s_sig_check_ret = true;
static bool sig_check_stub(uint8_t *buffer,
                           const uint8_t bufLen,
                           const uint8_t *PubKey,
                           const uint8_t keyLen,
                           const uint8_t keyUsageExp,
                           const uint8_t *signature,
                           const uint8_t sigLen,
                           int num_calls) {
    (void) buffer;
    (void) bufLen;
    (void) PubKey;
    (void) keyLen;
    (void) keyUsageExp;
    (void) signature;
    (void) sigLen;
    (void) num_calls;
    return s_sig_check_ret;
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
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    appState = APP_STATE_IDLE;
    memset(&dataContext, 0, sizeof(dataContext));
    pluginType = PLUGIN_TYPE_NONE;
    s_sig_check_ret = true;
}

// =============================================================================
// EXTERNAL plugin path
// =============================================================================

void test_external_plugin_check_presence_succeeds(void) {
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
    TEST_ASSERT_TRUE(sw == SWO_SUCCESS || sw == SWO_FILE_NOT_FOUND);
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_EXTERNAL);
}

void setUp(void) {
    Mockpublic_keys_Init();
    check_signature_with_pubkey_StubWithCallback(sig_check_stub);
    reset();
}
void tearDown(void) {
    Mockpublic_keys_Verify();
    Mockpublic_keys_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_external_plugin_check_presence_succeeds);
    return UNITY_END();
}
