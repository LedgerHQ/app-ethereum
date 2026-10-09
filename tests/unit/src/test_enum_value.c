/**
 * @file test_enum_value.c
 * @brief Unit tests for the enum-value backend descriptor at
 *        src/features/provide_enum_value/enum_value.c.
 *
 * An enum-value descriptor is the backend-signed mapping that says
 * "for contract X on chain Y, parameter slot ID, when the on-the-wire
 * value is N, display the label NAME". The device uses this to render
 * a meaningful name where it would otherwise display a raw uint8.
 *
 * A bug in the parser or lookup lets an attacker put any name in
 * front of any value/contract combination — e.g. "Standard execution"
 * could be displayed for a value that actually means "Delegate call
 * via $attacker_module". The signature gate against
 * CERTIFICATE_PUBLIC_KEY_USAGE_CALLDATA is the load-bearing defense.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "enum_value.h"
#include "Mockpublic_keys.h"
#include "Mockhash_bytes.h"

// =============================================================================
// Globals
// =============================================================================

// =============================================================================
// Controllable stubs
// =============================================================================

// check_signature_with_pubkey and finalize_hash are CMock-generated mocks.
// Control their return values via the local static variables below.
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

static bool s_finalize_hash_ret = true;
static bool finalize_hash_stub(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len, int num_calls) {
    (void) hash_ctx;
    (void) out;
    (void) out_len;
    (void) num_calls;
    return s_finalize_hash_ret;
}

// get_implem_contract is the proxy resolver. operation_key_matches consults it to remap a proxy
// address to its implementation before comparing against the registered contract_addr.
static const uint8_t *g_implem_contract_ret = NULL;
static int g_implem_contract_calls = 0;
const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *addr,
                                   const uint8_t *selector) {
    (void) chain_id;
    (void) addr;
    (void) selector;
    g_implem_contract_calls++;
    return g_implem_contract_ret;
}

// =============================================================================
// Test data
// =============================================================================

#define CONTRACT_BYTES                                                                        \
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, \
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA

static const uint8_t g_contract_addr[ADDRESS_LENGTH] = {CONTRACT_BYTES};
// a function of the contract, on chain 1
static const s_operation_key g_function = {.context = OPERATION_INFO_CTX_SIGN_TX,
                                           .chain_id = 1,
                                           .contract_addr = {CONTRACT_BYTES},
                                           .selector = {0xDE, 0xAD, 0xBE, 0xEF}};
// a primary type of a domain, whose hash starts with the bytes of the selector above
static const s_operation_key g_type = {
    .context = OPERATION_INFO_CTX_EIP712,
    .domain_separator = {0xD0, 0xD1, 0xD2, 0xD3},
    .primary_type_hash = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                          0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
                          0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C}};

// =============================================================================
// TLV builder
// =============================================================================

enum {
    WIRE_TAG_CHAIN_ID = 0x01,
    WIRE_TAG_CONTRACT_ADDR = 0x02,
    WIRE_TAG_SELECTOR = 0x03,
    WIRE_TAG_ID = 0x04,
    WIRE_TAG_DOMAIN_SEPARATOR = 0x07,
    WIRE_TAG_PRIMARY_TYPE_HASH = 0x08,
};

// Leave a tag out of the card
#define OMIT(tag) (1U << (tag))

typedef struct {
    uint8_t version;
    const s_operation_key *key;  // what the value is attached to, by the tags of its kind
    uint32_t omit;
    uint8_t extra_tag;  // 0: none, else a tag added with the bytes below
    const uint8_t *extra_value;
    uint8_t extra_size;
    uint8_t id;
    uint8_t value;
    const char *name;
    uint8_t sig_len;
} s_opts;

// A valid card for a key
static s_opts card(const s_operation_key *key) {
    return (s_opts) {.version = 0x01,
                     .key = key,
                     .id = 5,
                     .value = 2,
                     .name = "Standard",
                     .sig_len = 16};
}

static void put(uint8_t *out,
                size_t *off,
                uint32_t omit,
                uint8_t tag,
                const void *value,
                size_t size) {
    if (omit & OMIT(tag)) {
        return;
    }
    out[(*off)++] = tag;
    out[(*off)++] = (uint8_t) size;
    memcpy(out + *off, value, size);
    *off += size;
}

static size_t build_tlv(uint8_t *out, size_t out_size, s_opts opts) {
    const s_operation_key *key = opts.key;
    uint8_t chain_id = (uint8_t) key->chain_id;
    size_t off = 0;
    size_t name_len = strlen(opts.name);

    put(out, &off, 0, 0x00, &opts.version, 1);
    if (key->context == OPERATION_INFO_CTX_SIGN_TX) {
        put(out, &off, opts.omit, WIRE_TAG_CHAIN_ID, &chain_id, 1);
        put(out, &off, opts.omit, WIRE_TAG_CONTRACT_ADDR, key->contract_addr, ADDRESS_LENGTH);
        put(out, &off, opts.omit, WIRE_TAG_SELECTOR, key->selector, sizeof(key->selector));
    } else {
        put(out, &off, opts.omit, WIRE_TAG_DOMAIN_SEPARATOR, key->domain_separator, INT256_LENGTH);
        put(out,
            &off,
            opts.omit,
            WIRE_TAG_PRIMARY_TYPE_HASH,
            key->primary_type_hash,
            INT256_LENGTH);
    }
    put(out, &off, opts.omit, WIRE_TAG_ID, &opts.id, 1);
    put(out, &off, 0, 0x05, &opts.value, 1);
    put(out, &off, 0, 0x06, opts.name, name_len);
    if (opts.extra_tag != 0) {
        put(out, &off, 0, opts.extra_tag, opts.extra_value, opts.extra_size);
    }
    // 0xFF SIGNATURE — DER long-form for tag ≥ 0x80
    out[off++] = 0x81;
    out[off++] = 0xFF;
    out[off++] = opts.sig_len;
    memset(out + off, 0x42, opts.sig_len);
    off += opts.sig_len;
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

static bool run(s_opts opts) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    s_enum_value_ctx ctx = {0};
    buffer_t buf = {.ptr = tlv, .size = len, .offset = 0};

    if (!handle_enum_value_tlv_payload(&buf, &ctx)) return false;
    return verify_enum_value_struct(&ctx);
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    enum_value_cleanup();
    s_sig_check_ret = true;
    s_finalize_hash_ret = true;
    g_implem_contract_ret = NULL;
    g_implem_contract_calls = 0;
}

// =============================================================================
// Tests
// =============================================================================

void test_happy_path_registers_entry(void) {
    TEST_ASSERT_TRUE(run(card(&g_function)));
    const s_enum_value_entry *e = get_matching_enum(&g_function, 5, 2);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING(e->name, "Standard");
}

void test_lookup_no_registration_returns_null(void) {
    TEST_ASSERT_NULL(get_matching_enum(&g_function, 0, 0));
}

void test_invalid_version_rejected(void) {
    s_opts opts = card(&g_function);

    opts.version = 0x07;  // not STRUCT_VERSION
    TEST_ASSERT_FALSE(run(opts));
}

void test_signature_failure_rejects(void) {
    s_sig_check_ret = false;
    TEST_ASSERT_FALSE(run(card(&g_function)));
    TEST_ASSERT_NULL(get_matching_enum(&g_function, 5, 2));
}

void test_finalize_hash_failure_rejects(void) {
    s_finalize_hash_ret = false;
    TEST_ASSERT_FALSE(run(card(&g_function)));
}

void test_signature_too_short_rejected(void) {
    s_opts opts = card(&g_function);

    opts.sig_len = 4;  // < CX_ECDSA_SHA256_SIG_MIN_ASN1_LENGTH
    TEST_ASSERT_FALSE(run(opts));
}

void test_missing_required_field_rejected(void) {
    s_opts opts = card(&g_function);

    opts.omit = OMIT(WIRE_TAG_ID);
    TEST_ASSERT_FALSE(run(opts));
}

void test_lookup_chain_mismatch_returns_null(void) {
    s_operation_key other = g_function;

    TEST_ASSERT_TRUE(run(card(&g_function)));
    other.chain_id = 137;
    TEST_ASSERT_NULL(get_matching_enum(&other, 5, 2));
}

void test_lookup_address_mismatch_returns_null(void) {
    s_operation_key other = g_function;

    TEST_ASSERT_TRUE(run(card(&g_function)));
    memset(other.contract_addr, 0xCC, ADDRESS_LENGTH);
    TEST_ASSERT_NULL(get_matching_enum(&other, 5, 2));
}

void test_lookup_id_value_mismatch_returns_null(void) {
    TEST_ASSERT_TRUE(run(card(&g_function)));
    // Right (contract, selector, chain) but wrong (id, value).
    TEST_ASSERT_NULL(get_matching_enum(&g_function, 99, 2));
    TEST_ASSERT_NULL(get_matching_enum(&g_function, 5, 99));
}

void test_lookup_consults_proxy_resolver(void) {
    // Register an entry for the *implementation* address. Looking up
    // with the *proxy* address triggers get_implem_contract which
    // remaps to impl — the match must succeed.
    s_operation_key through_proxy = g_function;

    TEST_ASSERT_TRUE(run(card(&g_function)));
    // Map (any_proxy, selector, chain) -> g_contract_addr.
    g_implem_contract_ret = g_contract_addr;
    memset(through_proxy.contract_addr, 0xCC, ADDRESS_LENGTH);
    TEST_ASSERT_NOT_NULL(get_matching_enum(&through_proxy, 5, 2));
}

void test_two_entries_lookup_returns_matching_one(void) {
    s_opts a = card(&g_function);
    s_opts b = card(&g_function);

    a.value = 0;
    a.name = "Zero";
    b.value = 1;
    b.name = "One";
    TEST_ASSERT_TRUE(run(a));
    TEST_ASSERT_TRUE(run(b));
    const s_enum_value_entry *e0 = get_matching_enum(&g_function, 5, 0);
    const s_enum_value_entry *e1 = get_matching_enum(&g_function, 5, 1);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_STRING(e0->name, "Zero");
    TEST_ASSERT_EQUAL_STRING(e1->name, "One");
}

void test_enum_value_cleanup_releases_list(void) {
    TEST_ASSERT_TRUE(run(card(&g_function)));
    TEST_ASSERT_NOT_NULL(get_matching_enum(&g_function, 5, 2));
    enum_value_cleanup();
    TEST_ASSERT_NULL(get_matching_enum(&g_function, 5, 2));
}

// =============================================================================
// What a value is attached to: a function of a contract, or a primary type of a domain
// =============================================================================

// the tags of the card tell its kind, whatever operation is being described
void test_value_of_a_primary_type_registered_for_a_message(void) {
    TEST_ASSERT_TRUE(run(card(&g_type)));
    const s_enum_value_entry *e = get_matching_enum(&g_type, 5, 2);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING("Standard", e->name);
}

// the domain separator covers the chain and the contract of a message: no other domain matches
void test_value_of_a_primary_type_is_for_its_domain_and_type_only(void) {
    s_operation_key other = g_type;

    TEST_ASSERT_TRUE(run(card(&g_type)));
    other.domain_separator[INT256_LENGTH - 1] ^= 0x01;
    TEST_ASSERT_NULL(get_matching_enum(&other, 5, 2));
    other = g_type;
    other.primary_type_hash[INT256_LENGTH - 1] ^= 0x01;
    TEST_ASSERT_NULL(get_matching_enum(&other, 5, 2));
}

void test_key_of_another_size_refused(void) {
    static const uint8_t bytes[INT256_LENGTH + 1] = {0};
    const struct {
        const s_operation_key *key;
        uint8_t tag;
        uint8_t size;
    } cases[] = {
        {&g_function, WIRE_TAG_SELECTOR, INT256_LENGTH},
        {&g_type, WIRE_TAG_DOMAIN_SEPARATOR, CALLDATA_SELECTOR_SIZE},
        {&g_type, WIRE_TAG_PRIMARY_TYPE_HASH, INT256_LENGTH + 1},
    };

    for (size_t i = 0; i < ARRAYLEN(cases); i++) {
        s_opts opts = card(cases[i].key);

        opts.omit = OMIT(cases[i].tag);
        opts.extra_tag = cases[i].tag;
        opts.extra_value = bytes;
        opts.extra_size = cases[i].size;
        TEST_ASSERT_FALSE(run(opts));
    }
}

// a value is attached to a function of a contract or to a primary type of a domain: it has all the
// tags of one, and none of the other
void test_card_not_of_one_operation_refused(void) {
    static const uint8_t bytes[INT256_LENGTH] = {0x01};
    const struct {
        const s_operation_key *key;
        uint32_t omit;
        uint8_t extra_tag;
        uint8_t extra_size;
    } cases[] = {
        {&g_function, OMIT(WIRE_TAG_CHAIN_ID), 0, 0},
        {&g_function, OMIT(WIRE_TAG_CONTRACT_ADDR), 0, 0},
        {&g_function, OMIT(WIRE_TAG_SELECTOR), 0, 0},
        {&g_function, 0, WIRE_TAG_DOMAIN_SEPARATOR, INT256_LENGTH},
        {&g_function, 0, WIRE_TAG_PRIMARY_TYPE_HASH, INT256_LENGTH},
        {&g_type, OMIT(WIRE_TAG_DOMAIN_SEPARATOR), 0, 0},
        {&g_type, OMIT(WIRE_TAG_PRIMARY_TYPE_HASH), 0, 0},
        {&g_type, 0, WIRE_TAG_CHAIN_ID, 1},
        {&g_type, 0, WIRE_TAG_CONTRACT_ADDR, ADDRESS_LENGTH},
        {&g_type, 0, WIRE_TAG_SELECTOR, CALLDATA_SELECTOR_SIZE},
        {&g_type, OMIT(WIRE_TAG_DOMAIN_SEPARATOR) | OMIT(WIRE_TAG_PRIMARY_TYPE_HASH), 0, 0},
    };

    for (size_t i = 0; i < ARRAYLEN(cases); i++) {
        s_opts opts = card(cases[i].key);

        opts.omit = cases[i].omit;
        opts.extra_tag = cases[i].extra_tag;
        opts.extra_value = bytes;
        opts.extra_size = cases[i].extra_size;
        TEST_ASSERT_FALSE(run(opts));
    }
}

// a value of a function is not one of a primary type, and the reverse, whatever their bytes
void test_values_of_a_function_and_of_a_type_are_not_interchangeable(void) {
    TEST_ASSERT_TRUE(run(card(&g_function)));
    TEST_ASSERT_NULL(get_matching_enum(&g_type, 5, 2));
    enum_value_cleanup();

    TEST_ASSERT_TRUE(run(card(&g_type)));
    TEST_ASSERT_NULL(get_matching_enum(&g_function, 5, 2));
}

// a typed message names no proxy
void test_proxy_resolver_not_consulted_for_a_message(void) {
    TEST_ASSERT_TRUE(run(card(&g_type)));
    g_implem_contract_ret = g_contract_addr;
    TEST_ASSERT_NOT_NULL(get_matching_enum(&g_type, 5, 2));
    TEST_ASSERT_EQUAL(0, g_implem_contract_calls);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mockpublic_keys_Init();
    check_signature_with_pubkey_StubWithCallback(sig_check_stub);
    Mockhash_bytes_Init();
    finalize_hash_StubWithCallback(finalize_hash_stub);
    hash_nbytes_Ignore();
    hash_byte_Ignore();
    reset();
}

void tearDown(void) {
    Mockpublic_keys_Verify();
    Mockpublic_keys_Destroy();
    Mockhash_bytes_Verify();
    Mockhash_bytes_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_happy_path_registers_entry);
    RUN_TEST(test_lookup_no_registration_returns_null);
    RUN_TEST(test_invalid_version_rejected);
    RUN_TEST(test_signature_failure_rejects);
    RUN_TEST(test_finalize_hash_failure_rejects);
    RUN_TEST(test_signature_too_short_rejected);
    RUN_TEST(test_missing_required_field_rejected);
    RUN_TEST(test_lookup_chain_mismatch_returns_null);
    RUN_TEST(test_lookup_address_mismatch_returns_null);
    RUN_TEST(test_lookup_id_value_mismatch_returns_null);
    RUN_TEST(test_lookup_consults_proxy_resolver);
    RUN_TEST(test_two_entries_lookup_returns_matching_one);
    RUN_TEST(test_enum_value_cleanup_releases_list);
    RUN_TEST(test_value_of_a_primary_type_registered_for_a_message);
    RUN_TEST(test_value_of_a_primary_type_is_for_its_domain_and_type_only);
    RUN_TEST(test_key_of_another_size_refused);
    RUN_TEST(test_card_not_of_one_operation_refused);
    RUN_TEST(test_values_of_a_function_and_of_a_type_are_not_interchangeable);
    RUN_TEST(test_proxy_resolver_not_consulted_for_a_message);
    return UNITY_END();
}
