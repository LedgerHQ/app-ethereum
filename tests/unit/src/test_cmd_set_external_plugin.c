/**
 * @file test_cmd_set_external_plugin.c
 * @brief Unit tests for the SET_EXTERNAL_PLUGIN handler at
 *        src/features/set_external_plugin/cmd_set_external_plugin.c.
 *
 * SET_EXTERNAL_PLUGIN registers a (contract, selector) tuple as
 * handled by a named external plugin so the device can route
 * subsequent calldata-bearing transactions to the right plugin
 * binary. The host-supplied payload is signed by a Ledger PKI key.
 * A bug here lets an attacker pair an attacker-controlled plugin
 * name with a victim contract address, hijacking how the user sees
 * subsequent dApp interactions.
 *
 * The handler at-a-glance:
 *   - reject empty plugin name (defense in depth on top of the PKI
 *     signature, which a backend should never sign for an empty
 *     name anyway),
 *   - reject payloads too small to hold name + address + selector,
 *   - reject name lengths that would overrun the storage slot,
 *   - reject signatures that don't verify,
 *   - on success: load the plugin via os_lib_call, copy address +
 *     selector into dataContext.tokenContext, set pluginType =
 *     EXTERNAL, and bind to PLUGIN_CHAIN_ID_ANY (intentional: the
 *     external-plugin descriptor is chain-unbound, see the in-source
 *     comment about the CWE-345 follow-up).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "eth_plugin_internal.h"
#include "Mockpublic_keys.h"

// =============================================================================
// Globals required by linked translation units
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// check_signature_with_pubkey is a CMock-generated mock.
// Control its return value via s_sig_check_ret below.
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
// SDK exception scaffolding — same approach as test_cmd_set_plugin
// =============================================================================
//
// The TRY/CATCH path is only entered for external-plugin loading via
// os_lib_call. We don't exercise the THROW branch from these tests
// because driving a real longjmp through the SDK try-context chain
// would require reproducing the SDK's struct try_context_s layout
// faithfully. Provide enough stubs to satisfy the linker on the happy
// path; CATCH_OTHER stays unreachable.

static int g_os_lib_calls = 0;
void os_lib_call(unsigned int *params) {
    (void) params;
    g_os_lib_calls++;
}

// =============================================================================
// APDU payload builder
// =============================================================================
//
//   [name_len:1] [name:N] [address:20] [selector:4] [signature:M]
//

typedef struct {
    uint8_t name_len;       // wire byte
    const char *name;       // may be NULL → zero-fill
    uint8_t address_byte;   // 20 copies
    uint8_t selector_byte;  // 4 copies
    uint8_t sig_len;
    bool include_signature;
} s_opts;

static s_opts default_opts(void) {
    s_opts o = {.name_len = 4,
                .name = "Beef",
                .address_byte = 0xAA,
                .selector_byte = 0xCC,
                .sig_len = 64,
                .include_signature = true};
    return o;
}

static size_t build_apdu(uint8_t *out, size_t out_size, const s_opts *opts) {
    size_t off = 0;
    out[off++] = opts->name_len;
    if (opts->name != NULL) {
        for (uint8_t i = 0; i < opts->name_len; i++) {
            out[off++] = (uint8_t) opts->name[i];
        }
    } else {
        memset(out + off, 0, opts->name_len);
        off += opts->name_len;
    }
    memset(out + off, opts->address_byte, 20);
    off += 20;
    memset(out + off, opts->selector_byte, 4);
    off += 4;
    if (opts->include_signature) {
        for (uint8_t i = 0; i < opts->sig_len; i++) {
            out[off++] = 0x42;
        }
    }
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
    g_os_lib_calls = 0;
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
}

// =============================================================================
// Tests
// =============================================================================

void test_zero_length_payload_rejected(void) {
    uint8_t apdu[1] = {0};
    uint16_t sw = handle_set_external_plugin(apdu, 0);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_empty_plugin_name_rejected(void) {
    uint8_t apdu[100];
    s_opts opts = default_opts();
    opts.name_len = 0;
    opts.name = "";
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    TEST_ASSERT_EQUAL(check_signature_with_pubkey_CallCount(), 0);
}

void test_payload_too_small_rejected(void) {
    uint8_t apdu[100];
    s_opts opts = default_opts();
    opts.include_signature = false;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    // The source guards `dataLength <= payload_size` (strict !) so a
    // payload of *exactly* payload_size (no signature attached) still
    // fails the check.
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_name_too_long_rejected(void) {
    uint8_t apdu[256];
    s_opts opts = default_opts();
    // pluginName slot length is PLUGIN_NAME_MAX_LEN; the guard rejects
    // anything that would not leave room for the NUL terminator.
    opts.name_len = sizeof(dataContext.tokenContext.pluginName);
    opts.name = NULL;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_signature_failure_rejected(void) {
    s_sig_check_ret = false;
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    // The plugin must NOT have been activated.
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_NONE);
}

void test_happy_path_stores_address_and_selector(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(g_os_lib_calls, 1);
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_EXTERNAL);
    // pluginName NUL-terminated.
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginName[opts.name_len], '\0');
    TEST_ASSERT_EQUAL_MEMORY(dataContext.tokenContext.pluginName, "Beef", 4);
    // 20 bytes of 0xAA in contractAddress.
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_EQUAL(dataContext.tokenContext.contractAddress[i], 0xAA);
    }
    // 4 bytes of 0xCC in methodSelector.
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(dataContext.tokenContext.methodSelector[i], 0xCC);
    }
}

void test_happy_path_binds_chain_id_any(void) {
    // External-plugin enrollment is intentionally chain-unbound. Pin
    // that so a future "tighten chain binding" patch that touches this
    // file flips an explicit test rather than silently changing UX.
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    (void) handle_set_external_plugin(apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginChainId, PLUGIN_CHAIN_ID_ANY);
}

void test_rejected_when_app_not_idle(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    appState = APP_STATE_SIGNING_TX;
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    TEST_ASSERT_EQUAL(sw, SWO_COMMAND_NOT_ALLOWED);
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_NONE);
}

// =============================================================================
// Runner
// =============================================================================

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
    RUN_TEST(test_zero_length_payload_rejected);
    RUN_TEST(test_empty_plugin_name_rejected);
    RUN_TEST(test_payload_too_small_rejected);
    RUN_TEST(test_name_too_long_rejected);
    RUN_TEST(test_signature_failure_rejected);
    RUN_TEST(test_happy_path_stores_address_and_selector);
    RUN_TEST(test_happy_path_binds_chain_id_any);
    RUN_TEST(test_rejected_when_app_not_idle);
    return UNITY_END();
}
