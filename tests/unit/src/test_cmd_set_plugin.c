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

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"  // handle_set_plugin
#include "cmd_set_plugin.h"
#include "Mockpublic_keys.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Controllable stubs
// =============================================================================

static bool g_chain_compatible_ret = true;
bool app_compatible_with_chain_id(const uint64_t *chain_id) {
    (void) chain_id;
    return g_chain_compatible_ret;
}

// check_signature_with_pubkey is mocked via Mockpublic_keys.h (CMock).
// s_sig_check_ret controls the return value; use the callback stub so it
// can be reconfigured per-test without double-queuing IgnoreAndReturn.
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
// Fixture
// =============================================================================

static void reset(void) {
    appState = APP_STATE_IDLE;
    memset(&dataContext, 0, sizeof(dataContext));
    pluginType = PLUGIN_TYPE_NONE;
    g_chain_compatible_ret = true;
    s_sig_check_ret = true;
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
        TEST_ASSERT_TRUE(off <= out_size);
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
        TEST_ASSERT_TRUE(off <= out_size);
        return off;
    }
    out[off++] = opts.sig_len;
    size_t sig_to_write = opts.omit_sig_bytes ? 0 : opts.sig_len;
    memset(out + off, 0x42, sig_to_write);
    off += sig_to_write;
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

// =============================================================================
// Tests
// =============================================================================

void test_happy_path_erc721_registers(void) {
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
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_ERC721);
    TEST_ASSERT_EQUAL_STRING(dataContext.tokenContext.pluginName, "ERC721");
    TEST_ASSERT_EQUAL_MEMORY(dataContext.tokenContext.contractAddress, g_contract, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(dataContext.tokenContext.methodSelector, g_selector, SELECTOR_SIZE);
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginChainId, 1);
}

void test_happy_path_erc1155_registers(void) {
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
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_ERC1155);
    TEST_ASSERT_EQUAL(dataContext.tokenContext.pluginChainId, 137);
}

void test_header_too_small_rejected(void) {
    uint8_t payload[8] = {0x01, 0x01, 0x06};  // only HEADER_SIZE bytes
    uint16_t sw = handle_set_plugin(payload, 3);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_unsupported_type_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0xFF,  // not ETH_PLUGIN
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_unsupported_version_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x07,  // not VERSION_1
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_data_too_small_for_payload_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0x01, .version = 0x01, .name = "ERC721", .omit_after_header = true};
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_plugin_name_too_long_rejected(void) {
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
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_incompatible_chain_id_rejected(void) {
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
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_wrong_key_id_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x00,  // TEST key not allowed without HAVE_NFT_STAGING_KEY
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_wrong_algo_id_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x07,  // not SECG_P256K1+SHA256
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_signature_length_below_minimum_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 66};  // < MIN_DER_SIG_SIZE=67
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_signature_length_above_maximum_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 73};  // > MAX_DER_SIG_SIZE=72
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_data_too_short_for_signature_rejected(void) {
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
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_signature_check_failure_rejects(void) {
    s_sig_check_ret = false;
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "ERC721",
                   .chain_id = 1,
                   .key_id = 0x02,
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

// The PROD key is restricted to ERC721/ERC1155: registering an
// arbitrary external plugin name with the PROD key must be rejected.
void test_prod_key_with_external_plugin_rejected(void) {
    uint8_t payload[256];
    s_opts opts = {.type = 0x01,
                   .version = 0x01,
                   .name = "Uniswap",  // neither ERC721 nor ERC1155
                   .chain_id = 1,
                   .key_id = 0x02,  // PROD
                   .algo_id = 0x01,
                   .sig_len = 70};
    size_t len = build_payload(payload, sizeof(payload), opts);
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) len), SWO_INCORRECT_DATA);
}

void test_set_swap_with_calldata_plugin_type(void) {
    pluginType = PLUGIN_TYPE_NONE;
    set_swap_with_calldata_plugin_type();
    TEST_ASSERT_EQUAL(pluginType, PLUGIN_TYPE_SWAP_WITH_CALLDATA);
}

// Data large enough to clear the dataLength <= HEADER_SIZE guard (line 87)
// but too short to hold the full payload (line 112). Constructed by hand
// because build_payload's `omit_after_header` writes only 3 bytes -- which
// stops at the earlier HEADER_SIZE check.
void test_data_too_small_for_full_payload_rejected(void) {
    uint8_t payload[16] = {0};
    payload[0] = 0x01;  // type ETH_PLUGIN
    payload[1] = 0x01;  // version VERSION_1
    payload[2] = 6;     // plugin name length
    memcpy(payload + 3, "ERC721", 6);
    // Total length 9: > HEADER_SIZE=3 but << expected payloadSize (~43).
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, 9), SWO_INCORRECT_DATA);
}

// Payload exactly covers up to ALGORITHM_ID but truncates BEFORE the
// signature-length byte (line 172-174 check fires).
void test_data_too_short_for_sig_length_byte_rejected(void) {
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
    TEST_ASSERT_EQUAL(handle_set_plugin(payload, (uint8_t) off), SWO_INCORRECT_DATA);
}

// EXTERNAL-plugin BEGIN_TRY block is covered by a dedicated target
// test_cmd_set_plugin_staging (HAVE_NFT_STAGING_KEY defined so the
// non-PROD key is accepted -- the PROD key forbids non-NFT plugins).

void test_rejected_when_app_not_idle(void) {
    uint8_t payload[8] = {0x01, 0x01, 0x06};
    appState = APP_STATE_SIGNING_TX;
    uint16_t sw = handle_set_plugin(payload, 3);
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
    RUN_TEST(test_happy_path_erc721_registers);
    RUN_TEST(test_happy_path_erc1155_registers);
    RUN_TEST(test_header_too_small_rejected);
    RUN_TEST(test_unsupported_type_rejected);
    RUN_TEST(test_unsupported_version_rejected);
    RUN_TEST(test_data_too_small_for_payload_rejected);
    RUN_TEST(test_plugin_name_too_long_rejected);
    RUN_TEST(test_incompatible_chain_id_rejected);
    RUN_TEST(test_wrong_key_id_rejected);
    RUN_TEST(test_wrong_algo_id_rejected);
    RUN_TEST(test_signature_length_below_minimum_rejected);
    RUN_TEST(test_signature_length_above_maximum_rejected);
    RUN_TEST(test_data_too_short_for_signature_rejected);
    RUN_TEST(test_signature_check_failure_rejects);
    RUN_TEST(test_prod_key_with_external_plugin_rejected);
    RUN_TEST(test_set_swap_with_calldata_plugin_type);
    RUN_TEST(test_data_too_small_for_full_payload_rejected);
    RUN_TEST(test_data_too_short_for_sig_length_byte_rejected);
    RUN_TEST(test_rejected_when_app_not_idle);
    return UNITY_END();
}
