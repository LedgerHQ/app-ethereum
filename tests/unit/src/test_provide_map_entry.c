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
#include "operation_ctx.h"
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

static bool g_check_signature_with_pubkey_ret = true;
static const void *g_get_implem_contract_ret = NULL;
static int g_get_implem_contract_calls = 0;

// The operation being described, which an entry is found for by its key
static s_operation_key g_operation_key;
static bool g_operation_key_ret = true;

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

bool get_current_operation_key(s_operation_key *key) {
    if (!g_operation_key_ret) {
        return false;
    }
    *key = g_operation_key;
    return true;
}

const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *contract_addr,
                                   const uint8_t *selector) {
    (void) chain_id;
    (void) contract_addr;
    (void) selector;
    g_get_implem_contract_calls++;
    return (const uint8_t *) g_get_implem_contract_ret;
}

// =============================================================================
// Test data
// =============================================================================

#define CONTRACT_ADDR_BYTES                                                                   \
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, \
        0x00, 0xAA, 0xBB, 0xCC, 0xDD
#define SELECTOR_BYTES 0xAB, 0xCD, 0xEF, 0x01
// starts with the bytes of the selector: the two kinds of key must not be mistaken for each other
#define TYPE_HASH_BYTES                                                                           \
    0xAB, 0xCD, 0xEF, 0x01, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,     \
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, \
        0x2B, 0x2C
#define DOMAIN_SEPARATOR_BYTES                                                                    \
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,     \
        0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, \
        0xEE, 0xEF
#define KEY_BYTES   0xFF, 0xEE, 0xDD
#define VALUE_BYTES 'H', 'e', 'l', 'l', 'o'

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

// The same entry, attached to a primary type of a domain instead of a function of a contract
static const uint8_t s_message_payload[] = {
    0x00, 0x01, 0x01,                      // VERSION = 1
    0x07, 0x20, DOMAIN_SEPARATOR_BYTES,    // DOMAIN_SEPARATOR (32 bytes)
    0x08, 0x20, TYPE_HASH_BYTES,           // PRIMARY_TYPE_HASH (32 bytes)
    0x04, 0x01, 0x02,                      // ID = 2
    0x05, 0x03, KEY_BYTES,                 // KEY (3 bytes)
    0x06, 0x05, VALUE_BYTES,               // VALUE (5 bytes "Hello")
    0x81, 0xff, 0x0a,                      // SIGNATURE
    0x00, 0x00, 0x00, 0x00, 0x00,
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

#define VERSION_TAG      0x00, 0x01, 0x01
#define CHAIN_TAG        0x01, 0x01, 0x01
#define CONTRACT_TAG     0x02, 0x14, CONTRACT_ADDR_BYTES
#define SELECTOR_TAG     0x03, 0x04, SELECTOR_BYTES
#define SEPARATOR_TAG    0x07, 0x20, DOMAIN_SEPARATOR_BYTES
#define TYPE_HASH_TAG    0x08, 0x20, TYPE_HASH_BYTES
#define ENTRY_TAIL                                                                              \
    0x04, 0x01, 0x02,                      /* ID */                                             \
    0x05, 0x03, KEY_BYTES,                 /* KEY */                                            \
    0x06, 0x05, VALUE_BYTES,               /* VALUE */                                          \
    0x81, 0xff, 0x0a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* SIGNATURE */

// a key has the size of its kind
static const uint8_t s_payload_selector_as_type_hash[] = {
    VERSION_TAG, SEPARATOR_TAG, 0x08, 0x04, SELECTOR_BYTES, ENTRY_TAIL,
};
static const uint8_t s_payload_type_hash_as_selector[] = {
    VERSION_TAG, CHAIN_TAG, CONTRACT_TAG, 0x03, 0x20, TYPE_HASH_BYTES, ENTRY_TAIL,
};
// an entry is for a function of a contract or for a primary type of a domain: it has all the tags of
// one, and none of the other
static const uint8_t s_payload_function_and_type[] = {
    VERSION_TAG, CHAIN_TAG, CONTRACT_TAG, SELECTOR_TAG, SEPARATOR_TAG, TYPE_HASH_TAG, ENTRY_TAIL,
};
static const uint8_t s_payload_type_with_a_chain[] = {
    VERSION_TAG, CHAIN_TAG, SEPARATOR_TAG, TYPE_HASH_TAG, ENTRY_TAIL,
};
static const uint8_t s_payload_function_without_contract[] = {
    VERSION_TAG, CHAIN_TAG, SELECTOR_TAG, ENTRY_TAIL,
};
static const uint8_t s_payload_type_without_domain[] = {
    VERSION_TAG, TYPE_HASH_TAG, ENTRY_TAIL,
};
static const uint8_t s_payload_no_key[] = {
    VERSION_TAG, ENTRY_TAIL,
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

    TEST_ASSERT_EQUAL(OPERATION_INFO_CTX_SIGN_TX, ctx.entry.operation_key.context);
    TEST_ASSERT_EQUAL(ctx.entry.operation_key.chain_id, 1);
    TEST_ASSERT_EQUAL_MEMORY(ctx.entry.operation_key.contract_addr,
                             s_contract_addr,
                             ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(ctx.entry.operation_key.selector,
                             ((uint8_t[]) {SELECTOR_BYTES}),
                             CALLDATA_SELECTOR_SIZE);
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

// The operation being described is a transaction, which an entry is attached to by its function
static void be_in_a_transaction(void) {
    static const uint8_t selector[] = {SELECTOR_BYTES};

    memset(&g_operation_key, 0, sizeof(g_operation_key));
    g_operation_key.context = OPERATION_INFO_CTX_SIGN_TX;
    g_operation_key.chain_id = 1;
    memcpy(g_operation_key.contract_addr, s_contract_addr, ADDRESS_LENGTH);
    memcpy(g_operation_key.selector, selector, sizeof(selector));
    g_get_implem_contract_ret = NULL;  // no proxy
}

void test_map_entry_lookup_no_tx_info(void) {
    g_operation_key_ret = false;

    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

void test_map_entry_lookup_no_selector(void) {
    be_in_a_transaction();
    g_operation_key_ret = false;

    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

void test_map_entry_lookup_found(void) {
    // The lookup matches the callee of the current (possibly nested) context
    memcpy(g_tx_content.destination, s_contract_addr, ADDRESS_LENGTH);
    txContext.content = &g_tx_content;

    register_entry();
    be_in_a_transaction();

    uint8_t key[] = {KEY_BYTES};
    const s_map_entry *entry = get_matching_map_entry(2, key, sizeof(key));
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL(entry->value_size, 5);
    TEST_ASSERT_EQUAL_MEMORY(entry->value, "Hello", 5);
}

void test_map_entry_lookup_wrong_key(void) {
    memcpy(g_tx_content.destination, s_contract_addr, ADDRESS_LENGTH);
    txContext.content = &g_tx_content;

    register_entry();
    be_in_a_transaction();

    uint8_t wrong_key[] = {0x11, 0x22, 0x33};
    TEST_ASSERT_NULL(get_matching_map_entry(2, wrong_key, sizeof(wrong_key)));
}

void test_map_entry_lookup_wrong_id(void) {
    memcpy(g_tx_content.destination, s_contract_addr, ADDRESS_LENGTH);
    txContext.content = &g_tx_content;

    register_entry();
    be_in_a_transaction();

    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(99, key, sizeof(key)));  // wrong id
}

void test_map_entry_cleanup(void) {
    register_entry();

    map_entry_cleanup();

    // List is now empty
    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

// =============================================================================
// Test cases — the key an entry is attached to
// =============================================================================

// The operation being described is a typed message, which an entry is attached to by its type
static void be_in_a_message(void) {
    static const uint8_t domain_separator[] = {DOMAIN_SEPARATOR_BYTES};
    static const uint8_t type_hash[] = {TYPE_HASH_BYTES};

    memset(&g_operation_key, 0, sizeof(g_operation_key));
    g_operation_key.context = OPERATION_INFO_CTX_EIP712;
    memcpy(g_operation_key.domain_separator, domain_separator, sizeof(domain_separator));
    memcpy(g_operation_key.primary_type_hash, type_hash, sizeof(type_hash));
}

static bool receive(const uint8_t *payload, size_t size) {
    s_map_entry_ctx ctx = {0};
    buffer_t buf = {.ptr = (uint8_t *) payload, .size = size, .offset = 0};

    if (!handle_map_entry_tlv_payload(&buf, &ctx)) {
        return false;
    }
    return verify_map_entry_struct(&ctx);
}

// the tags of the entry tell its kind, whatever operation is being described
void test_map_entry_received_with_no_operation(void) {
    g_operation_key_ret = false;
    TEST_ASSERT_TRUE(receive(s_valid_payload, sizeof(s_valid_payload)));
    TEST_ASSERT_TRUE(receive(s_message_payload, sizeof(s_message_payload)));
}

void test_map_entry_key_of_another_size_refused(void) {
    TEST_ASSERT_FALSE(
        receive(s_payload_selector_as_type_hash, sizeof(s_payload_selector_as_type_hash)));
    TEST_ASSERT_FALSE(
        receive(s_payload_type_hash_as_selector, sizeof(s_payload_type_hash_as_selector)));
}

void test_map_entry_not_of_one_operation_refused(void) {
    TEST_ASSERT_FALSE(receive(s_payload_function_and_type, sizeof(s_payload_function_and_type)));
    TEST_ASSERT_FALSE(receive(s_payload_type_with_a_chain, sizeof(s_payload_type_with_a_chain)));
    TEST_ASSERT_FALSE(
        receive(s_payload_function_without_contract, sizeof(s_payload_function_without_contract)));
    TEST_ASSERT_FALSE(
        receive(s_payload_type_without_domain, sizeof(s_payload_type_without_domain)));
    TEST_ASSERT_FALSE(receive(s_payload_no_key, sizeof(s_payload_no_key)));
}

void test_map_entry_lookup_found_for_a_message(void) {
    be_in_a_message();
    TEST_ASSERT_TRUE(receive(s_message_payload, sizeof(s_message_payload)));

    uint8_t key[] = {KEY_BYTES};
    const s_map_entry *entry = get_matching_map_entry(2, key, sizeof(key));
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL(OPERATION_INFO_CTX_EIP712, entry->operation_key.context);
    TEST_ASSERT_EQUAL_MEMORY(entry->value, "Hello", 5);
}

// the domain separator covers the chain and the contract of a message: no other domain matches
void test_map_entry_for_a_message_not_found_for_another_domain(void) {
    be_in_a_message();
    TEST_ASSERT_TRUE(receive(s_message_payload, sizeof(s_message_payload)));

    g_operation_key.domain_separator[INT256_LENGTH - 1] ^= 0x01;
    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

// a message names no proxy
void test_map_entry_lookup_ignores_proxy_for_a_message(void) {
    be_in_a_message();
    TEST_ASSERT_TRUE(receive(s_message_payload, sizeof(s_message_payload)));

    g_get_implem_contract_ret = s_contract_addr;
    g_get_implem_contract_calls = 0;
    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NOT_NULL(get_matching_map_entry(2, key, sizeof(key)));
    TEST_ASSERT_EQUAL(0, g_get_implem_contract_calls);
}

// an entry for a primary type is not found for a function, even when the bytes start the same
void test_map_entry_for_a_message_not_found_for_a_transaction(void) {
    be_in_a_message();
    TEST_ASSERT_TRUE(receive(s_message_payload, sizeof(s_message_payload)));

    be_in_a_transaction();
    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

void test_map_entry_for_a_transaction_not_found_for_a_message(void) {
    TEST_ASSERT_TRUE(receive(s_valid_payload, sizeof(s_valid_payload)));

    be_in_a_message();
    uint8_t key[] = {KEY_BYTES};
    TEST_ASSERT_NULL(get_matching_map_entry(2, key, sizeof(key)));
}

// =============================================================================
// Test runner
// =============================================================================

void setUp(void) {
    g_check_signature_with_pubkey_ret = true;
    be_in_a_transaction();
    g_operation_key_ret = true;
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
    RUN_TEST(test_map_entry_received_with_no_operation);
    RUN_TEST(test_map_entry_key_of_another_size_refused);
    RUN_TEST(test_map_entry_not_of_one_operation_refused);
    RUN_TEST(test_map_entry_lookup_found_for_a_message);
    RUN_TEST(test_map_entry_for_a_message_not_found_for_another_domain);
    RUN_TEST(test_map_entry_lookup_ignores_proxy_for_a_message);
    RUN_TEST(test_map_entry_for_a_message_not_found_for_a_transaction);
    RUN_TEST(test_map_entry_for_a_transaction_not_found_for_a_message);
    return UNITY_END();
}
