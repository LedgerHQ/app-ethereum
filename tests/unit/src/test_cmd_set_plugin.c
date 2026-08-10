/**
 * @file test_cmd_set_plugin.c
 * @brief Unit tests for the external-plugin registration APDU at
 *        src/features/set_plugin/cmd_set_plugin.c.
 *
 * INS_SET_PLUGIN carries a backend-signed binary payload that
 * registers a plugin for the next transaction: TYPE + VERSION +
 * NAME_LEN + NAME + CONTRACT_ADDR + SELECTOR + CHAIN_ID + KEY_ID +
 * ALGO_ID + SIG_LEN + SIG. After validation the device sets
 * pluginType to ERC721, ERC1155 or EXTERNAL and caches the
 * (contract, selector, chain_id) tuple that eth_plugin_perform_init
 * later cross-checks (the cross-check fires app_exit on mismatch —
 * already covered in test_eth_plugin_handler).
 *
 * A faulty parser here lets a hostile host register an arbitrary
 * plugin alias for any (contract, selector) and the device will
 * display the matching plugin UI for whatever bytes the user signs
 * next.
 *
 * The PROD key restriction (only ERC721/ERC1155 can be registered
 * with the PROD signing key, not arbitrary external plugins) is the
 * load-bearing defense against an attacker repurposing the
 * production signing key.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"  // handle_set_plugin
#include "cmd_set_plugin.h"
#include "wraps.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Controllable stubs
// =============================================================================

static bool g_chain_compatible_ret = true;
bool __wrap_app_compatible_with_chain_id(const uint64_t *chain_id) {
    (void) chain_id;
    return g_chain_compatible_ret;
}

// check_signature_with_pubkey is wrapped in mocks/mock.c; state via
// g_sig_check_ret from wraps.h.

// =============================================================================
// Fixture
// =============================================================================

static int reset(void **state) {
    (void) state;
    appState = APP_STATE_IDLE;
    memset(&dataContext, 0, sizeof(dataContext));
    pluginType = PLUGIN_TYPE_NONE;
    g_chain_compatible_ret = true;
    g_sig_check_ret = true;
    return 0;
}

// =============================================================================
// Binary payload builder
// =============================================================================
//
// Layout (no TLV; flat binary):
//   type(1)        = 0x01 ETH_PLUGIN
//   version(1)     = 0x01 VERSION_1
//   name_len(1)
//   name(name_len)
//   contract_addr(20)
//   selector(4)
//   chain_id(8) BE
//   key_id(1)      = 0x02 PROD_PLUGIN_KEY (or 0x00 TEST)
//   algo_id(1)     = 0x01 ECC_SECG_P256K1__ECDSA_SHA_256
//   sig_len(1)
//   sig(sig_len)

typedef struct {
    uint8_t type;
    uint8_t version;
    const char *name;
    uint64_t chain_id;
    uint8_t key_id;
    uint8_t algo_id;
    uint8_t sig_len;
    // toggles
    bool omit_after_header;  // truncate after NAME_LEN
    bool omit_sig_len_byte;  // truncate before sig_len
    bool omit_sig_bytes;     // truncate inside sig
} s_opts;

static const uint8_t g_contract[ADDRESS_LENGTH] = {
    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
    0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
};
static const uint8_t g_selector[SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

static size_t build_payload(uint8_t *out, size_t out_size, s_opts opts) {
    size_t off = 0;
    out[off++] = opts.type;
    out[off++] = opts.version;
    size_t name_len = (opts.name != NULL) ? strlen(opts.name) : 0;
    out[off++] = (uint8_t) name_len;
    if (opts.omit_after_header) {
        assert_true(off <= out_size);
        return off;
    }
    if (name_len > 0) {
        memcpy(out + off, opts.name, name_len);
        off += name_len;
    }
    memcpy(out + off, g_contract, ADDRESS_LENGTH);
    off += ADDRESS_LENGTH;
    memcpy(out + off, g_selector, SELECTOR_SIZE);
    off += SELECTOR_SIZE;
    // chain_id BE 8 bytes
    for (int i = 7; i >= 0; --i) {
        out[off++] = (uint8_t) ((opts.chain_id >> (i * 8)) & 0xFF);
    }
    out[off++] = opts.key_id;
    out[off++] = opts.algo_id;
    if (opts.omit_sig_len_byte) {
        assert_true(off <= out_size);
        return off;
    }
    out[off++] = opts.sig_len;
    size_t sig_to_write = opts.omit_sig_bytes ? 0 : opts.sig_len;
    memset(out + off, 0x42, sig_to_write);
    off += sig_to_write;
    assert_true(off <= out_size);
    return off;
}

// =============================================================================
// Tests
// =============================================================================

static void test_happy_path_erc721_registers(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    uint16_t sw = handle_set_plugin(payload, (uint8_t) len);
    assert_int_equal(sw, SWO_SUCCESS);
    assert_int_equal(pluginType, PLUGIN_TYPE_ERC721);
    assert_string_equal(dataContext.tokenContext.pluginName, "ERC721");
    assert_memory_equal(dataContext.tokenContext.contractAddress, g_contract, ADDRESS_LENGTH);
    assert_memory_equal(dataContext.tokenContext.methodSelector, g_selector, SELECTOR_SIZE);
    assert_int_equal(dataContext.tokenContext.pluginChainId, 1);
}

static void test_happy_path_erc1155_registers(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC1155",
                   .chain_id = 137,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    uint16_t sw = handle_set_plugin(payload, (uint8_t) len);
    assert_int_equal(sw, SWO_SUCCESS);
    assert_int_equal(pluginType, PLUGIN_TYPE_ERC1155);
    assert_int_equal(dataContext.tokenContext.pluginChainId, 137);
}

static void test_header_too_small_rejected(void **state) {
    (void) state;
    uint8_t payload[8] = {0x01, 0x01, 0x06};  // only HEADER_SIZE bytes
    uint16_t sw = handle_set_plugin(payload, 3);
    assert_int_equal(sw, SWO_INCORRECT_DATA);
}

static void test_unsupported_type_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0xFF,  // not ETH_PLUGIN
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_unsupported_version_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x07,  // not VERSION_1
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_data_too_small_for_payload_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01, .version = 0x01, .name = "ERC721", .omit_after_header = true};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_plugin_name_too_long_rejected(void **state) {
    (void) state;
    // pluginName buffer size is PLUGIN_ID_LENGTH = 31 (or similar) — pick
    // a length safely larger.
    uint8_t payload[256];
    char long_name[64];
    memset(long_name, 'X', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = long_name,
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_incompatible_chain_id_rejected(void **state) {
    (void) state;
    g_chain_compatible_ret = false;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 99999,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_wrong_key_id_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x00,  // TEST key not allowed without HAVE_NFT_STAGING_KEY
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_wrong_algo_id_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x07,  // not SECG_P256K1+SHA256
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_signature_length_below_minimum_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 66};  // < MIN_DER_SIG_SIZE=67
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_signature_length_above_maximum_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 73};  // > MAX_DER_SIG_SIZE=72
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_data_too_short_for_signature_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70,
                   .omit_sig_bytes = true};  // declare 70 but provide 0
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_signature_check_failure_rejects(void **state) {
    (void) state;
    g_sig_check_ret = false;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

// The PROD key is restricted to ERC721/ERC1155: registering an
// arbitrary external plugin name with the PROD key must be rejected.
static void test_prod_key_with_external_plugin_rejected(void **state) {
    (void) state;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "Uniswap",  // neither ERC721 nor ERC1155
                   .chain_id = 1,
                   .key_id = 0x02,  // PROD
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    assert_int_equal(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

static void test_set_swap_with_calldata_plugin_type(void **state) {
    (void) state;
    pluginType = PLUGIN_TYPE_NONE;
    set_swap_with_calldata_plugin_type();
    assert_int_equal(pluginType, PLUGIN_TYPE_SWAP_WITH_CALLDATA);
}

// Data large enough to clear the dataLength <= HEADER_SIZE guard (line 87)
// but too short to hold the full payload (line 112). Constructed by hand
// because build_payload's `omit_after_header` writes only 3 bytes -- which
// stops at the earlier HEADER_SIZE check.
static void test_data_too_small_for_full_payload_rejected(void **state) {
    (void) state;
    uint8_t payload[16] = {0};
    payload[0] = 0x01;  // type ETH_PLUGIN
    payload[1] = 0x01;  // version VERSION_1
    payload[2] = 6;     // plugin name length
    memcpy(payload + 3, "ERC721", 6);
    // Total length 9: > HEADER_SIZE=3 but << expected payloadSize (~43).
    assert_int_equal(handle_set_plugin(payload, 9), SWO_INCORRECT_DATA);
}

// Payload exactly covers up to ALGORITHM_ID but truncates BEFORE the
// signature-length byte (line 172-174 check fires).
static void test_data_too_short_for_sig_length_byte_rejected(void **state) {
    (void) state;
    uint8_t payload[64] = {0};
    payload[0] = 0x01;  // type
    payload[1] = 0x01;  // version
    payload[2] = 6;     // name length
    memcpy(payload + 3, "ERC721", 6);
    // ADDRESS (20) + SELECTOR (4) + CHAIN_ID (8) + KEY_ID (1) + ALGO_ID (1)
    // chain_id = 1 (big-endian), key_id = PROD (0x02), algo = 0x01.
    size_t off = 9;
    memset(payload + off, 0xAB, 20);  // address
    off += 20;
    memset(payload + off, 0xCD, 4);  // selector
    off += 4;
    payload[off + 7] = 1;  // chain_id big-endian = 1
    off += 8;
    payload[off++] = 0x02;  // PROD key_id
    payload[off++] = 0x01;  // algo
    // payloadSize complete (43 bytes). dataLength = 43, no room for sig_len.
    assert_int_equal(handle_set_plugin(payload, (uint8_t) off), SWO_INCORRECT_DATA);
}

// EXTERNAL-plugin BEGIN_TRY block is covered by a dedicated target
// test_cmd_set_plugin_staging (HAVE_NFT_STAGING_KEY defined so the
// non-PROD key is accepted -- the PROD key forbids non-NFT plugins).

static void test_rejected_when_app_not_idle(void **state) {
    (void) state;
    uint8_t payload[8] = {0x01, 0x01, 0x06};
    appState = APP_STATE_SIGNING_TX;
    uint16_t sw = handle_set_plugin(payload, 3);
    assert_int_equal(sw, SWO_COMMAND_NOT_ALLOWED);
    assert_int_equal(pluginType, PLUGIN_TYPE_NONE);
}

// =============================================================================
// Runner
// =============================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_rejected_when_app_not_idle, reset),
        cmocka_unit_test_setup(test_happy_path_erc721_registers, reset),
        cmocka_unit_test_setup(test_happy_path_erc1155_registers, reset),
        cmocka_unit_test_setup(test_header_too_small_rejected, reset),
        cmocka_unit_test_setup(test_unsupported_type_rejected, reset),
        cmocka_unit_test_setup(test_unsupported_version_rejected, reset),
        cmocka_unit_test_setup(test_data_too_small_for_payload_rejected, reset),
        cmocka_unit_test_setup(test_plugin_name_too_long_rejected, reset),
        cmocka_unit_test_setup(test_incompatible_chain_id_rejected, reset),
        cmocka_unit_test_setup(test_wrong_key_id_rejected, reset),
        cmocka_unit_test_setup(test_wrong_algo_id_rejected, reset),
        cmocka_unit_test_setup(test_signature_length_below_minimum_rejected, reset),
        cmocka_unit_test_setup(test_signature_length_above_maximum_rejected, reset),
        cmocka_unit_test_setup(test_data_too_short_for_signature_rejected, reset),
        cmocka_unit_test_setup(test_signature_check_failure_rejects, reset),
        cmocka_unit_test_setup(test_prod_key_with_external_plugin_rejected, reset),
        cmocka_unit_test_setup(test_set_swap_with_calldata_plugin_type, reset),
        cmocka_unit_test_setup(test_data_too_small_for_full_payload_rejected, reset),
        cmocka_unit_test_setup(test_data_too_short_for_sig_length_byte_rejected, reset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
