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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "eth_plugin_internal.h"
#include "wraps.h"

// =============================================================================
// Globals required by linked translation units
// =============================================================================

// =============================================================================
// Wraps
// =============================================================================

// check_signature_with_pubkey is wrapped in mocks/mock.c; state via
// g_sig_check_ret + g_sig_check_calls from wraps.h.

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
    g_sig_check_calls = 0;
    g_os_lib_calls = 0;
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
    return 0;
}

// =============================================================================
// Tests
// =============================================================================

static void test_zero_length_payload_rejected(void **state) {
    (void) state;
    uint8_t apdu[1] = {0};
    uint16_t sw = handle_set_external_plugin(apdu, 0);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_empty_plugin_name_rejected(void **state) {
    (void) state;
    uint8_t apdu[100];
    s_opts opts = default_opts();
    opts.name_len = 0;
    opts.name = "";
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
    assert_int_equal(g_sig_check_calls, 0);
}

static void test_payload_too_small_rejected(void **state) {
    (void) state;
    uint8_t apdu[100];
    s_opts opts = default_opts();
    opts.include_signature = false;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    // The source guards `dataLength <= payload_size` (strict !) so a
    // payload of *exactly* payload_size (no signature attached) still
    // fails the check.
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_name_too_long_rejected(void **state) {
    (void) state;
    uint8_t apdu[256];
    s_opts opts = default_opts();
    // pluginName slot length is PLUGIN_NAME_MAX_LEN; the guard rejects
    // anything that would not leave room for the NUL terminator.
    opts.name_len = sizeof(dataContext.tokenContext.pluginName);
    opts.name = NULL;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_signature_failure_rejected(void **state) {
    (void) state;
    g_sig_check_ret = false;
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
    // The plugin must NOT have been activated.
    assert_int_equal(pluginType, PLUGIN_TYPE_NONE);
}

static void test_happy_path_stores_address_and_selector(void **state) {
    (void) state;
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    assert_int_equal(sw, SWO_SUCCESS);
    assert_int_equal(g_os_lib_calls, 1);
    assert_int_equal(pluginType, PLUGIN_TYPE_EXTERNAL);
    // pluginName NUL-terminated.
    assert_int_equal(dataContext.tokenContext.pluginName[opts.name_len], '\0');
    assert_memory_equal(dataContext.tokenContext.pluginName, "Beef", 4);
    // 20 bytes of 0xAA in contractAddress.
    for (int i = 0; i < 20; i++) {
        assert_int_equal(dataContext.tokenContext.contractAddress[i], 0xAA);
    }
    // 4 bytes of 0xCC in methodSelector.
    for (int i = 0; i < 4; i++) {
        assert_int_equal(dataContext.tokenContext.methodSelector[i], 0xCC);
    }
}

static void test_happy_path_binds_chain_id_any(void **state) {
    (void) state;
    // External-plugin enrollment is intentionally chain-unbound. Pin
    // that so a future "tighten chain binding" patch that touches this
    // file flips an explicit test rather than silently changing UX.
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    (void) handle_set_external_plugin(apdu, (uint8_t) len);
    assert_int_equal(dataContext.tokenContext.pluginChainId, PLUGIN_CHAIN_ID_ANY);
}

static void test_rejected_when_app_not_idle(void **state) {
    (void) state;
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    appState = APP_STATE_SIGNING_TX;
    uint16_t sw = handle_set_external_plugin(apdu, (uint8_t) len);
    assert_int_equal(sw, SWO_COMMAND_NOT_ALLOWED);
    assert_int_equal(pluginType, PLUGIN_TYPE_NONE);
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_rejected_when_app_not_idle, reset),
        cmocka_unit_test_setup(test_zero_length_payload_rejected, reset),
        cmocka_unit_test_setup(test_empty_plugin_name_rejected, reset),
        cmocka_unit_test_setup(test_payload_too_small_rejected, reset),
        cmocka_unit_test_setup(test_name_too_long_rejected, reset),
        cmocka_unit_test_setup(test_signature_failure_rejected, reset),
        cmocka_unit_test_setup(test_happy_path_stores_address_and_selector, reset),
        cmocka_unit_test_setup(test_happy_path_binds_chain_id_any, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
