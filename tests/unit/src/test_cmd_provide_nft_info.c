/**
 * @file test_cmd_provide_nft_info.c
 * @brief Unit tests for the NFT collection metadata descriptor at
 *        src/features/provide_nft_information/cmd_provide_nft_info.c.
 *
 * The host delivers (collection_name, contract_address, chain_id)
 * tuples signed by a Ledger key. Once accepted, the device pairs the
 * address+chain with the human-readable collection name and shows that
 * name to the user during NFT transfers. A bug in the parser or the
 * signature gate would let an attacker pair an arbitrary collection
 * name with any contract address — so the user sees "CryptoPunks"
 * while signing a transfer to a phishing contract.
 *
 * Pin every wire-format guard, the four header validations
 * (type / version / key_id / algo_id), the chain-id compatibility
 * check, the signature verification, the trailing-bytes guard, and
 * the storage contract (index in [0, 255], duplicate insert returns
 * the same index).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "apdu_constants.h"
#include "nft_info.h"
#include "asset_info.h"  // COLLECTION_NAME_MAX_LEN
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

static bool g_chain_compatible = true;
bool chain_id_is_signable(uint64_t chain_id) {
    (void) chain_id;
    return g_chain_compatible;
}

// =============================================================================
// APDU payload builder
// =============================================================================
//
//   [type:1] [version:1] [name_len:1] [name:name_len]
//   [address:20] [chain_id:8 BE] [key_id:1] [algo_id:1] [sig_len:1] [sig:sig_len]
//

typedef struct {
    uint8_t type;
    uint8_t version;
    uint8_t name_len;      // wire-side length byte (may differ from name_str length)
    const char *name;      // pointer to the actual name bytes
    uint8_t address_byte;  // 20 copies of this byte
    uint64_t chain_id;
    uint8_t key_id;
    uint8_t algo_id;
    uint8_t sig_len;
    bool include_signature;
    int8_t truncate_at;        // if >= 0, stop emitting bytes at this offset
    bool extra_trailing_byte;  // append one byte past the signature
} s_opts;

static s_opts default_opts(void) {
    s_opts o = {.type = 1,
                .version = 1,
                .name_len = 4,
                .name = "Boop",
                .address_byte = 0xAA,
                .chain_id = 1,
                .key_id = 1,  // PROD_NFT_METADATA_KEY (matches default build)
                .algo_id = 1,
                .sig_len = 64,
                .include_signature = true,
                .truncate_at = -1,
                .extra_trailing_byte = false};
    return o;
}

static size_t build_apdu(uint8_t *out, size_t out_size, const s_opts *opts) {
    size_t off = 0;
    out[off++] = opts->type;
    if (opts->truncate_at == 0) return off;
    out[off++] = opts->version;
    if (opts->truncate_at == 1) return off;
    out[off++] = opts->name_len;
    if (opts->truncate_at == 2) return off;
    if (opts->name != NULL) {
        for (uint8_t i = 0; i < opts->name_len; i++) {
            out[off++] = (uint8_t) opts->name[i];
        }
    } else {
        // No backing buffer — emit zero bytes for name_len iterations.
        memset(out + off, 0, opts->name_len);
        off += opts->name_len;
    }
    if (opts->truncate_at == 3) return off;
    memset(out + off, opts->address_byte, 20);
    off += 20;
    if (opts->truncate_at == 4) return off;
    for (int i = 7; i >= 0; i--) {
        out[off++] = (uint8_t) (opts->chain_id >> (8 * i));
    }
    if (opts->truncate_at == 5) return off;
    out[off++] = opts->key_id;
    if (opts->truncate_at == 6) return off;
    out[off++] = opts->algo_id;
    if (opts->truncate_at == 7) return off;
    if (opts->include_signature) {
        out[off++] = opts->sig_len;
        for (uint8_t i = 0; i < opts->sig_len; i++) {
            out[off++] = 0x42;
        }
    }
    if (opts->extra_trailing_byte) {
        out[off++] = 0xCC;
    }
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    clear_nft_infos();
    s_sig_check_ret = true;
    g_chain_compatible = true;
    memset(G_io_tx_buffer, 0, sizeof(G_io_tx_buffer));
}

// =============================================================================
// Tests — entry-point guards
// =============================================================================

void test_p1_nonzero_rejected(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(/*p1=*/1, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_P1_P2);
}

void test_p2_nonzero_rejected(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, /*p2=*/1, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_P1_P2);
}

// =============================================================================
// Tests — header validation
// =============================================================================

void test_type_invalid_rejected(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.type = 0x05;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_version_invalid_rejected(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.version = 0x02;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_collection_name_too_long_rejected(void) {
    // The struct field is COLLECTION_NAME_MAX_LEN + 1 wide and the guard
    // is `name_len + 1 > sizeof(info.collection_name)`. So a name of
    // COLLECTION_NAME_MAX_LEN + 1 bytes spills the NUL-terminator slot.
    uint8_t apdu[256];
    s_opts opts = default_opts();
    opts.name_len = COLLECTION_NAME_MAX_LEN + 1;
    opts.name = NULL;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_collection_name_at_max_accepted(void) {
    // name_len = COLLECTION_NAME_MAX_LEN must succeed (boundary case).
    uint8_t apdu[256];
    s_opts opts = default_opts();
    opts.name_len = COLLECTION_NAME_MAX_LEN;
    opts.name = NULL;  // emit zero bytes
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
}

void test_unsupported_chain_id_rejected(void) {
    g_chain_compatible = false;
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    // No signature check for an unsupported chain.
    TEST_ASSERT_EQUAL(check_signature_with_pubkey_CallCount(), 0);
}

void test_unexpected_key_id_rejected(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.key_id = 0x05;  // not STAGING (0) or PROD (1)
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_unexpected_algo_id_rejected(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.algo_id = 0x02;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

// =============================================================================
// Tests — truncation guards
// =============================================================================

void test_truncated_missing_type(void) {
    uint8_t apdu[1] = {0};
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, /*lc=*/0, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_truncated_missing_version(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.truncate_at = 0;  // emit just `type`
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_truncated_missing_address(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.truncate_at = 3;  // type, version, name_len, name only
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_truncated_missing_chain_id(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.truncate_at = 4;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_truncated_missing_signature_length(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.truncate_at = 7;  // up to algo_id, then stop
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_truncated_signature_underflow(void) {
    // Declare sig_len = 64 but stop after the length byte.
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.sig_len = 64;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    // Trim the buffer length down to just below the signature payload.
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) (len - opts.sig_len), apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

void test_extra_trailing_bytes_rejected(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    opts.extra_trailing_byte = true;
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
}

// =============================================================================
// Tests — signature & storage
// =============================================================================

void test_signature_failure_does_not_store(void) {
    s_sig_check_ret = false;
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_INCORRECT_DATA);
    // get_matching_nft_info must NOT find the (chain, address) pair.
    uint8_t addr[20];
    memset(addr, 0xAA, sizeof(addr));
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_nft_info(&chain, addr));
}

void test_happy_path_stores_and_returns_index(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(tx, 1);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);  // first insert → index 0
    // Round-trip through the public getter.
    uint8_t addr[20];
    memset(addr, 0xAA, sizeof(addr));
    uint64_t chain = 1;
    const s_nft_info *got = get_matching_nft_info(&chain, addr);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_MEMORY(got->collection_name, "Boop", 4);
    TEST_ASSERT_EQUAL(got->chain_id, 1);
}

void test_duplicate_insert_returns_same_index(void) {
    uint8_t apdu[200];
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);

    unsigned int tx = 0;
    (void) handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);

    // Re-send the same descriptor — must hit the existing-node branch
    // in set_nft_info and report the same index.
    tx = 0;
    memset(G_io_tx_buffer, 0xCC, 4);  // sentinel to detect overwrite
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);
}

void test_second_descriptor_gets_next_index(void) {
    uint8_t apdu[200];

    // First descriptor on chain 1, address 0xAA.
    s_opts opts = default_opts();
    size_t len = build_apdu(apdu, sizeof(apdu), &opts);
    unsigned int tx = 0;
    (void) handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 0);

    // Second descriptor on chain 1, address 0xBB → distinct slot.
    opts.address_byte = 0xBB;
    opts.name = "Doop";
    opts.name_len = 4;
    len = build_apdu(apdu, sizeof(apdu), &opts);
    tx = 0;
    uint16_t sw = handle_provide_nft_information(0, 0, (uint8_t) len, apdu, &tx);
    TEST_ASSERT_EQUAL(sw, SWO_SUCCESS);
    TEST_ASSERT_EQUAL(G_io_tx_buffer[0], 1);
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
    RUN_TEST(test_p1_nonzero_rejected);
    RUN_TEST(test_p2_nonzero_rejected);
    RUN_TEST(test_type_invalid_rejected);
    RUN_TEST(test_version_invalid_rejected);
    RUN_TEST(test_collection_name_too_long_rejected);
    RUN_TEST(test_collection_name_at_max_accepted);
    RUN_TEST(test_unsupported_chain_id_rejected);
    RUN_TEST(test_unexpected_key_id_rejected);
    RUN_TEST(test_unexpected_algo_id_rejected);
    RUN_TEST(test_truncated_missing_type);
    RUN_TEST(test_truncated_missing_version);
    RUN_TEST(test_truncated_missing_address);
    RUN_TEST(test_truncated_missing_chain_id);
    RUN_TEST(test_truncated_missing_signature_length);
    RUN_TEST(test_truncated_signature_underflow);
    RUN_TEST(test_extra_trailing_bytes_rejected);
    RUN_TEST(test_signature_failure_does_not_store);
    RUN_TEST(test_happy_path_stores_and_returns_index);
    RUN_TEST(test_duplicate_insert_returns_same_index);
    RUN_TEST(test_second_descriptor_gets_next_index);
    return UNITY_END();
}
