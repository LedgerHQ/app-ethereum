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

// get_implem_contract is the proxy resolver. is_matching_enum consults
// it to remap a proxy address to its implementation before comparing
// against the registered contract_addr.
static const uint8_t *g_implem_contract_ret = NULL;
const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *addr,
                                   const uint8_t *selector) {
    (void) chain_id;
    (void) addr;
    (void) selector;
    return g_implem_contract_ret;
}

// =============================================================================
// Test data
// =============================================================================

static const uint8_t g_contract_addr[ADDRESS_LENGTH] = {
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};
static const uint8_t g_selector[SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

// =============================================================================
// TLV builder
// =============================================================================

typedef struct {
    uint8_t version;
    uint8_t chain_id;
    const uint8_t *contract_addr;
    const uint8_t *selector;
    uint8_t id;
    uint8_t value;
    const char *name;
    uint8_t sig_len;
    bool omit_id;
} s_opts;

static size_t build_tlv(uint8_t *out, size_t out_size, s_opts opts) {
    size_t off = 0;
    // 0x00 VERSION
    out[off++] = 0x00;
    out[off++] = 0x01;
    out[off++] = opts.version;
    // 0x01 CHAIN_ID
    out[off++] = 0x01;
    out[off++] = 0x01;
    out[off++] = opts.chain_id;
    // 0x02 CONTRACT_ADDR
    out[off++] = 0x02;
    out[off++] = ADDRESS_LENGTH;
    memcpy(out + off, opts.contract_addr, ADDRESS_LENGTH);
    off += ADDRESS_LENGTH;
    // 0x03 SELECTOR
    out[off++] = 0x03;
    out[off++] = SELECTOR_SIZE;
    memcpy(out + off, opts.selector, SELECTOR_SIZE);
    off += SELECTOR_SIZE;
    if (!opts.omit_id) {
        // 0x04 ID
        out[off++] = 0x04;
        out[off++] = 0x01;
        out[off++] = opts.id;
    }
    // 0x05 VALUE
    out[off++] = 0x05;
    out[off++] = 0x01;
    out[off++] = opts.value;
    // 0x06 NAME
    size_t name_len = strlen(opts.name);
    out[off++] = 0x06;
    out[off++] = (uint8_t) name_len;
    memcpy(out + off, opts.name, name_len);
    off += name_len;
    // 0xFF SIGNATURE — DER long-form for tag ≥ 0x80
    out[off++] = 0x81;
    out[off++] = 0xFF;
    out[off++] = opts.sig_len;
    memset(out + off, 0x42, opts.sig_len);
    off += opts.sig_len;
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

static bool run(const uint8_t *tlv, size_t len) {
    s_enum_value_ctx ctx = {0};
    buffer_t buf = {.ptr = (uint8_t *) tlv, .size = len, .offset = 0};
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
}

// =============================================================================
// Tests
// =============================================================================

void test_happy_path_registers_entry(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "Standard",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run(tlv, len));
    uint64_t chain = 1;
    const s_enum_value_entry *e = get_matching_enum(&chain, g_contract_addr, g_selector, 5, 2);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_STRING(e->name, "Standard");
}

void test_lookup_no_registration_returns_null(void) {
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_enum(&chain, g_contract_addr, g_selector, 0, 0));
}

void test_invalid_version_rejected(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x07,  // not STRUCT_VERSION
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "X",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run(tlv, len));
}

void test_signature_failure_rejects(void) {
    s_sig_check_ret = false;
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "X",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run(tlv, len));
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_enum(&chain, g_contract_addr, g_selector, 5, 2));
}

void test_finalize_hash_failure_rejects(void) {
    s_finalize_hash_ret = false;
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "X",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run(tlv, len));
}

void test_signature_too_short_rejected(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "X",
                   .sig_len = 4};  // < CX_ECDSA_SHA256_SIG_MIN_ASN1_LENGTH
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run(tlv, len));
}

void test_missing_required_field_rejected(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .value = 2,
                   .name = "X",
                   .omit_id = true,  // missing TAG_ID
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run(tlv, len));
}

void test_lookup_chain_mismatch_returns_null(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "Standard",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run(tlv, len));
    uint64_t wrong_chain = 137;
    TEST_ASSERT_NULL(get_matching_enum(&wrong_chain, g_contract_addr, g_selector, 5, 2));
}

void test_lookup_address_mismatch_returns_null(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "Standard",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run(tlv, len));
    uint8_t wrong_addr[ADDRESS_LENGTH];
    memset(wrong_addr, 0xCC, ADDRESS_LENGTH);
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_matching_enum(&chain, wrong_addr, g_selector, 5, 2));
}

void test_lookup_id_value_mismatch_returns_null(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "Standard",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run(tlv, len));
    uint64_t chain = 1;
    // Right (contract, selector, chain) but wrong (id, value).
    TEST_ASSERT_NULL(get_matching_enum(&chain, g_contract_addr, g_selector, 99, 2));
    TEST_ASSERT_NULL(get_matching_enum(&chain, g_contract_addr, g_selector, 5, 99));
}

void test_lookup_consults_proxy_resolver(void) {
    // Register an entry for the *implementation* address. Looking up
    // with the *proxy* address triggers get_implem_contract which
    // remaps to impl — the match must succeed.
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "Standard",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run(tlv, len));

    // Map (any_proxy, selector, chain) -> g_contract_addr.
    g_implem_contract_ret = g_contract_addr;
    uint8_t proxy_addr[ADDRESS_LENGTH] = {0xCC};
    uint64_t chain = 1;
    const s_enum_value_entry *e = get_matching_enum(&chain, proxy_addr, g_selector, 5, 2);
    TEST_ASSERT_NOT_NULL(e);
}

void test_two_entries_lookup_returns_matching_one(void) {
    uint8_t tlv[256];
    s_opts a = {.version = 0x01,
                .chain_id = 1,
                .contract_addr = g_contract_addr,
                .selector = g_selector,
                .id = 5,
                .value = 0,
                .name = "Zero",
                .sig_len = 16};
    s_opts b = {.version = 0x01,
                .chain_id = 1,
                .contract_addr = g_contract_addr,
                .selector = g_selector,
                .id = 5,
                .value = 1,
                .name = "One",
                .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), a);
    TEST_ASSERT_TRUE(run(tlv, len));
    len = build_tlv(tlv, sizeof(tlv), b);
    TEST_ASSERT_TRUE(run(tlv, len));
    uint64_t chain = 1;
    const s_enum_value_entry *e0 = get_matching_enum(&chain, g_contract_addr, g_selector, 5, 0);
    const s_enum_value_entry *e1 = get_matching_enum(&chain, g_contract_addr, g_selector, 5, 1);
    TEST_ASSERT_NOT_NULL(e0);
    TEST_ASSERT_NOT_NULL(e1);
    TEST_ASSERT_EQUAL_STRING(e0->name, "Zero");
    TEST_ASSERT_EQUAL_STRING(e1->name, "One");
}

void test_enum_value_cleanup_releases_list(void) {
    uint8_t tlv[256];
    s_opts opts = {.version = 0x01,
                   .chain_id = 1,
                   .contract_addr = g_contract_addr,
                   .selector = g_selector,
                   .id = 5,
                   .value = 2,
                   .name = "Standard",
                   .sig_len = 16};
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run(tlv, len));
    uint64_t chain = 1;
    TEST_ASSERT_NOT_NULL(get_matching_enum(&chain, g_contract_addr, g_selector, 5, 2));
    enum_value_cleanup();
    TEST_ASSERT_NULL(get_matching_enum(&chain, g_contract_addr, g_selector, 5, 2));
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
    return UNITY_END();
}
