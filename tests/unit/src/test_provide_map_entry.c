/**
 * @file test_provide_map_entry.c
 * @brief Unit tests for MAP_ENTRY TLV parsing, verification, and lookup
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Module under test
#include "map_entry.h"

// Headers for mocked functions
#include "public_keys.h"
#include "hash_bytes.h"
#include "shared_context.h"
#include "eth_ustream.h"
#include "tx_ctx.h"
#include "calldata.h"
#include "proxy_info.h"
#include "utils.h"
#include "common_utils.h"
#include "wraps.h"  // extern g_tx_content

// =============================================================================
// Global stubs
// =============================================================================

// Contract address used in all TLV payloads — must match CONTRACT_ADDR_BYTES below
static const uint8_t s_contract_addr[ADDRESS_LENGTH] = {
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD,
};

static const void *g_calldata_get_selector_ret = NULL;
static bool g_check_signature_with_pubkey_ret = true;
static const void *g_get_current_calldata_ret = NULL;
static const void *g_get_current_tx_info_ret = NULL;
static const void *g_get_implem_contract_ret = NULL;
static const void *g_get_current_tx_to_ret = NULL;

// =============================================================================
// Mock functions
// =============================================================================

static bool s_finalize_hash_ret = true;
bool finalize_hash(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len) {
    (void) hash_ctx;
    memset(out, 0, out_len);
    return (bool) s_finalize_hash_ret;
}

bool check_signature_with_pubkey(uint8_t *buffer,
                                 const uint8_t bufLen,
                                 const uint8_t *PubKey,
                                 const uint8_t keyLen,
                                 const uint8_t keyUsageExp,
                                 const uint8_t *signature,
                                 const uint8_t sigLen) {
    (void) buffer;
    (void) bufLen;
    (void) PubKey;
    (void) keyLen;
    (void) keyUsageExp;
    (void) signature;
    (void) sigLen;
    return (bool) g_check_signature_with_pubkey_ret;
}

const s_tx_info *get_current_tx_info(void) {
    return (const s_tx_info *) g_get_current_tx_info_ret;
}

const uint8_t *get_current_tx_to(void) {
    return (const uint8_t *) g_get_current_tx_to_ret;
}

s_calldata *get_current_calldata(void) {
    return (s_calldata *) g_get_current_calldata_ret;
}

const uint8_t *calldata_get_selector(const s_calldata *calldata) {
    (void) calldata;
    return (const uint8_t *) g_calldata_get_selector_ret;
}

const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *contract_addr,
                                   const uint8_t *selector) {
    (void) chain_id;
    (void) contract_addr;
    (void) selector;
    return (const uint8_t *) g_get_implem_contract_ret;
}

// =============================================================================
// Test data
// =============================================================================

#define CONTRACT_ADDR_BYTES                                                                   \
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, \
        0x00, 0xAA, 0xBB, 0xCC, 0xDD
#define SELECTOR_BYTES 0xAB, 0xCD, 0xEF, 0x01
#define KEY_BYTES      0xFF, 0xEE, 0xDD
#define VALUE_BYTES    'H', 'e', 'l', 'l', 'o'

// clang-format off
// Tag 0xff is DER-encoded as 0x81 0xff (long form, since 0xff >= 0x80)
static const uint8_t s_valid_payload[] = {
    0x00, 0x01, 0x01,                      // VERSION = 1
    0x01, 0x01, 0x01,                      // CHAIN_ID = 1 (1 byte, minimal encoding)
    0x02, 0x14, CONTRACT_ADDR_BYTES,       // CONTRACT_ADDR (20 bytes)
    0x03, 0x04, SELECTOR_BYTES,            // SELECTOR (4 bytes)
    0x04, 0x01, 0x02,                      // ID = 2
    0x05, 0x03, KEY_BYTES,                 // KEY (3 bytes)
    0x06, 0x05, VALUE_BYTES,               // VALUE (5 bytes "Hello")
    0x81, 0xff, 0x0a,                      // SIGNATURE tag (DER: 0x81 0xff), len=10
    0x00, 0x00, 0x00, 0x00, 0x00,          // fake signature bytes
    0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t s_payload_no_value[] = {
    0x00, 0x01, 0x01,                      // VERSION
    0x01, 0x01, 0x01,                      // CHAIN_ID
    0x02, 0x14, CONTRACT_ADDR_BYTES,       // CONTRACT_ADDR
    0x03, 0x04, SELECTOR_BYTES,            // SELECTOR
    0x04, 0x01, 0x02,                      // ID
    0x05, 0x03, KEY_BYTES,                 // KEY
    // VALUE (0x06) missing
    0x81, 0xff, 0x0a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // SIGNATURE (DER tag)
};
// clang-format on

// =============================================================================
// Helpers
// =============================================================================

static void setup_empty_list(void) {
    map_entry_cleanup();
}

// Register one valid entry into the global list (with mocked signature success).
static void register_entry(void) {
    s_map_entry_ctx ctx = {0};
    buffer_t buf = {.ptr = (uint8_t *) s_valid_payload,
                    .size = sizeof(s_valid_payload),
                    .offset = 0};

    TEST_ASSERT_TRUE(handle_map_entry_tlv_payload(&buf, &ctx));
    s_finalize_hash_ret = true;
    g_check_signature_with_pubkey_ret = true;
    TEST_ASSERT_TRUE(verify_map_entry_struct(&ctx));
}

// =============================================================================
// Test cases — TLV parsing
// =============================================================================

void test_map_entry_parse_valid(void) {
    s_map_entry_ctx ctx = {0};
    buffer_t buf = {.ptr = (uint8_t *) s_valid_payload,
                    .size = sizeof(s_valid_payload),
                    .offset = 0};

    TEST_ASSERT_TRUE(handle_map_entry_tlv_payload(&buf, &ctx));

    TEST_ASSERT_EQUAL(ctx.entry.chain_id, 1);
    TEST_ASSERT_EQUAL_MEMORY(ctx.entry.contract_addr, s_contract_addr, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(ctx.entry.selector, ((uint8_t[]) {SELECTOR_BYTES}), SELECTOR_SIZE);
    TEST_ASSERT_EQUAL(ctx.entry.id, 2);
    TEST_ASSERT_EQUAL(ctx.entry.key_size, 3);
    TEST_ASSERT_EQUAL_MEMORY(ctx.entry.key, ((uint8_t[]) {KEY_BYTES}), 3);
    TEST_ASSERT_EQUAL(ctx.entry.value_size, 5);
    TEST_ASSERT_EQUAL_MEMORY(ctx.entry.value, "Hello", 5);
}

// =============================================================================
// Test cases — signature verification
// =============================================================================

void test_map_entry_verify_valid(void) {
    s_map_entry_ctx ctx = {0};
    buffer_t buf = {.ptr = (uint8_t *) s_valid_payload,
                    .size = sizeof(s_valid_payload),
                    .offset = 0};

    TEST_ASSERT_TRUE(handle_map_entry_tlv_payload(&buf, &ctx));

    s_finalize_hash_ret = true;
    g_check_signature_with_pubkey_ret = true;
    TEST_ASSERT_TRUE(verify_map_entry_struct(&ctx));
}

void test_map_entry_verify_bad_sig(void) {
    s_map_entry_ctx ctx = {0};
    buffer_t buf = {.ptr = (uint8_t *) s_valid_payload,
                    .size = sizeof(s_valid_payload),
                    .offset = 0};

    TEST_ASSERT_TRUE(handle_map_entry_tlv_payload(&buf, &ctx));

    s_finalize_hash_ret = true;
    g_check_signature_with_pubkey_ret = false;
    TEST_ASSERT_FALSE(verify_map_entry_struct(&ctx));
}

void test_map_entry_verify_missing_mandatory_tag(void) {
    // VALUE tag is absent → verify_fields() fails inside verify_map_entry_struct()
    s_map_entry_ctx ctx = {0};
    buffer_t buf = {.ptr = (uint8_t *) s_payload_no_value,
                    .size = sizeof(s_payload_no_value),
                    .offset = 0};

    TEST_ASSERT_TRUE(handle_map_entry_tlv_payload(&buf, &ctx));
    TEST_ASSERT_FALSE(verify_map_entry_struct(&ctx));
}

// =============================================================================
// Test cases — lookup
// =============================================================================

void test_map_entry_lookup_no_tx_info(void) {
    g_get_current_tx_info_ret = NULL;

    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

void test_map_entry_lookup_no_selector(void) {
    static const s_tx_info tx_info = {.chain_id = 1};

    g_get_current_tx_info_ret = &tx_info;
    g_get_current_calldata_ret = NULL;
    g_calldata_get_selector_ret = NULL;

    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

void test_map_entry_lookup_found(void) {
    static const s_tx_info tx_info = {.chain_id = 1};
    static const uint8_t selector[] = {SELECTOR_BYTES};

    // The lookup matches the callee of the current (possibly nested) context
    memcpy(g_tx_content.destination, s_contract_addr, ADDRESS_LENGTH);
    txContext.content = &g_tx_content;

    register_entry();

    g_get_current_tx_info_ret = &tx_info;
    g_get_current_tx_to_ret = s_contract_addr;
    g_get_current_calldata_ret = NULL;
    g_calldata_get_selector_ret = selector;
    g_get_implem_contract_ret = NULL;  // no proxy

    uint8_t key[] = {KEY_BYTES};
    const s_map_entry *entry = get_matching_map_entry(2, key, sizeof(key));
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL(entry->value_size, 5);
    TEST_ASSERT_EQUAL_MEMORY(entry->value, "Hello", 5);
}

void test_map_entry_lookup_wrong_key(void) {
    static const s_tx_info tx_info = {.chain_id = 1};
    static const uint8_t selector[] = {SELECTOR_BYTES};

    memcpy(g_tx_content.destination, s_contract_addr, ADDRESS_LENGTH);
    txContext.content = &g_tx_content;

    register_entry();

    g_get_current_tx_info_ret = &tx_info;
    g_get_current_calldata_ret = NULL;
    g_calldata_get_selector_ret = selector;
    g_get_implem_contract_ret = NULL;

    uint8_t wrong_key[] = {0x11, 0x22, 0x33};
    TEST_ASSERT_NULL(get_matching_map_entry(2, wrong_key, sizeof(wrong_key)));
}

void test_map_entry_lookup_wrong_id(void) {
    static const s_tx_info tx_info = {.chain_id = 1};
    static const uint8_t selector[] = {SELECTOR_BYTES};

    memcpy(g_tx_content.destination, s_contract_addr, ADDRESS_LENGTH);
    txContext.content = &g_tx_content;

    register_entry();

    g_get_current_tx_info_ret = &tx_info;
    g_get_current_calldata_ret = NULL;
    g_calldata_get_selector_ret = selector;
    g_get_implem_contract_ret = NULL;

    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(99, key, sizeof(key)));  // wrong id
}

void test_map_entry_cleanup(void) {
    register_entry();

    map_entry_cleanup();

    // List is now empty: get_matching_map_entry returns NULL immediately
    g_get_current_tx_info_ret = NULL;
    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
    g_calldata_get_selector_ret = NULL;
    g_check_signature_with_pubkey_ret = true;
    g_get_current_calldata_ret = NULL;
    g_get_current_tx_info_ret = NULL;
    g_get_implem_contract_ret = NULL;
    s_finalize_hash_ret = true;
    setup_empty_list();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_map_entry_parse_valid);
    RUN_TEST(test_map_entry_verify_valid);
    RUN_TEST(test_map_entry_verify_bad_sig);
    RUN_TEST(test_map_entry_verify_missing_mandatory_tag);
    RUN_TEST(test_map_entry_lookup_no_tx_info);
    RUN_TEST(test_map_entry_lookup_no_selector);
    RUN_TEST(test_map_entry_lookup_found);
    RUN_TEST(test_map_entry_lookup_wrong_key);
    RUN_TEST(test_map_entry_lookup_wrong_id);
    RUN_TEST(test_map_entry_cleanup);
    return UNITY_END();
}
