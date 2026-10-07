/**
 * @file test_proxy_info.c
 * @brief Unit tests for the proxy-info backend descriptor at
 *        src/features/provide_proxy_info/proxy_info.c.
 *
 * proxy_info caches the (chain_id, proxy_address -> implementation_address)
 * mapping signed by the backend so the device, when later parsing a
 * transaction whose `to` looks like a proxy, can resolve the actual
 * implementation contract and render the right plugin/UI for it.
 *
 * Two security gates carry the load:
 *   - verify_proxy_info_struct(): finalize the running keccak hash
 *     over every non-SIGNATURE TLV, then run check_signature_with_pubkey
 *     against the backend's CERTIFICATE_PUBLIC_KEY_USAGE_TRUSTED_NAME.
 *     Only on success is g_proxy_info populated with the descriptor
 *     contents — otherwise a hostile host could whisper any
 *     (proxy -> impl) mapping it wants.
 *   - check_proxy_params(): on lookup, the requested (chain_id, addr,
 *     selector) must match the registered descriptor exactly. A
 *     regression here would let a registered proxy resolution leak
 *     onto a different chain or a different selector — the user
 *     reviews "interact with $known_dapp" while signing for an
 *     attacker contract.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "proxy_info.h"
#include "Mockpublic_keys.h"
#include "Mockhash_bytes.h"

// =============================================================================
// Globals the module reads
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

static int g_roll_challenge_calls = 0;
void roll_challenge(void) {
    g_roll_challenge_calls++;
}

// =============================================================================
// Fixture
// =============================================================================

// Concrete test data
static const uint8_t g_proxy_addr[ADDRESS_LENGTH] = {
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};
static const uint8_t g_impl_addr[ADDRESS_LENGTH] = {
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
};
static const uint8_t g_selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

static void reset(void) {
    proxy_cleanup();
    s_sig_check_ret = true;
    s_finalize_hash_ret = true;
    g_roll_challenge_calls = 0;
}

// =============================================================================
// TLV payload builder. Each tag fits short-form (length < 128).
// =============================================================================
//
// Layout:
//   0x01 STRUCT_TYPE        len=1, value=0x26
//   0x02 STRUCT_VERSION     len=1, value=0x01
//   0x12 CHALLENGE          len=4, value=BE32
//   0x22 ADDRESS            len=20, proxy address
//   0x23 CHAIN_ID           len=1, chain_id LSB
//   0x41 SELECTOR           len=4, selector (optional — driven by include_selector)
//   0x42 IMPLEM_ADDRESS     len=20, impl address
//   0x43 DELEGATION_TYPE    len=1, value=delegation_type
//   0x15 SIGNATURE          len=sig_size, sig bytes (mock'd check)
//
// The minimum-valid signature size is CX_ECDSA_SHA256_SIG_MIN_ASN1_LENGTH = 8.

static size_t build_tlv(uint8_t *out,
                        size_t out_size,
                        uint8_t struct_type,
                        uint8_t version,
                        uint8_t chain_id,
                        const uint8_t *addr,
                        const uint8_t *impl,
                        bool include_selector,
                        uint8_t delegation_type,
                        uint8_t sig_len) {
    size_t off = 0;
    out[off++] = 0x01;
    out[off++] = 0x01;
    out[off++] = struct_type;
    out[off++] = 0x02;
    out[off++] = 0x01;
    out[off++] = version;
    out[off++] = 0x12;
    out[off++] = 0x04;
    out[off++] = 0xDE;
    out[off++] = 0xAD;
    out[off++] = 0xBE;
    out[off++] = 0xEF;  // CHALLENGE (mock check_challenge OK)
    out[off++] = 0x22;
    out[off++] = ADDRESS_LENGTH;
    memcpy(out + off, addr, ADDRESS_LENGTH);
    off += ADDRESS_LENGTH;
    out[off++] = 0x23;
    out[off++] = 0x01;
    out[off++] = chain_id;
    if (include_selector) {
        out[off++] = 0x41;
        out[off++] = CALLDATA_SELECTOR_SIZE;
        memcpy(out + off, g_selector, CALLDATA_SELECTOR_SIZE);
        off += CALLDATA_SELECTOR_SIZE;
    }
    out[off++] = 0x42;
    out[off++] = ADDRESS_LENGTH;
    memcpy(out + off, impl, ADDRESS_LENGTH);
    off += ADDRESS_LENGTH;
    out[off++] = 0x43;
    out[off++] = 0x01;
    out[off++] = delegation_type;
    out[off++] = 0x15;
    out[off++] = sig_len;
    memset(out + off, 0x42, sig_len);
    off += sig_len;
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

static bool run_parse_and_verify(const uint8_t *tlv, size_t len, s_proxy_info_ctx *ctx) {
    buffer_t buf = {.ptr = (uint8_t *) tlv, .size = len, .offset = 0};
    if (!handle_proxy_info_tlv_payload(&buf, ctx)) return false;
    return verify_proxy_info_struct(ctx);
}

// =============================================================================
// Tests
// =============================================================================

void test_happy_path_registers_descriptor(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv,
                           sizeof(tlv),
                           0x26,
                           0x01,
                           1,
                           g_proxy_addr,
                           g_impl_addr,
                           /*include_selector=*/true,
                           0 /*PROXY*/,
                           16);
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_TRUE(run_parse_and_verify(tlv, len, &ctx));
    // roll_challenge ran exactly once — anti-replay anchor.
    TEST_ASSERT_EQUAL(g_roll_challenge_calls, 1);
    // Lookups now resolve: get_implem_contract returns the impl address
    // when queried with the matching (chain, proxy_addr, selector).
    uint64_t chain = 1;
    const uint8_t *impl = get_implem_contract(&chain, g_proxy_addr, g_selector);
    TEST_ASSERT_NOT_NULL(impl);
    TEST_ASSERT_EQUAL_MEMORY(impl, g_impl_addr, ADDRESS_LENGTH);
    // Reverse lookup: get_proxy_contract(impl_addr) -> proxy_addr.
    const uint8_t *proxy = get_proxy_contract(&chain, g_impl_addr, g_selector);
    TEST_ASSERT_NOT_NULL(proxy);
    TEST_ASSERT_EQUAL_MEMORY(proxy, g_proxy_addr, ADDRESS_LENGTH);
}

void test_lookups_without_registered_descriptor_return_null(void) {
    // After proxy_cleanup (called in reset) g_proxy_info is NULL —
    // both lookups must short-circuit to NULL.
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_implem_contract(&chain, g_proxy_addr, g_selector));
    TEST_ASSERT_NULL(get_proxy_contract(&chain, g_impl_addr, g_selector));
}

void test_signature_check_failure_rejects(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_sig_check_ret = false;
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_FALSE(run_parse_and_verify(tlv, len, &ctx));
    // No descriptor must have been registered.
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_implem_contract(&chain, g_proxy_addr, g_selector));
}

void test_finalize_hash_failure_rejects(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_finalize_hash_ret = false;
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_FALSE(run_parse_and_verify(tlv, len, &ctx));
}

void test_invalid_struct_type_rejected(void) {
    uint8_t tlv[256];
    // Send STRUCT_TYPE = 0xFF (not 0x26 = TYPE_PROXY_INFO).
    size_t len = build_tlv(tlv, sizeof(tlv), 0xFF, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_proxy_info_ctx ctx = {0};
    buffer_t buf = {.ptr = tlv, .size = len, .offset = 0};
    TEST_ASSERT_FALSE(handle_proxy_info_tlv_payload(&buf, &ctx));
}

void test_invalid_struct_version_rejected(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x05, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_proxy_info_ctx ctx = {0};
    buffer_t buf = {.ptr = tlv, .size = len, .offset = 0};
    TEST_ASSERT_FALSE(handle_proxy_info_tlv_payload(&buf, &ctx));
}

void test_delegation_type_out_of_range_rejected(void) {
    uint8_t tlv[256];
    // DELEGATION_TYPE_MAX = 3, so any value >= 3 must be rejected.
    size_t len =
        build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0x7F, 16);
    s_proxy_info_ctx ctx = {0};
    buffer_t buf = {.ptr = tlv, .size = len, .offset = 0};
    TEST_ASSERT_FALSE(handle_proxy_info_tlv_payload(&buf, &ctx));
}

void test_signature_too_short_rejected(void) {
    uint8_t tlv[256];
    // sig_len = 4 < CX_ECDSA_SHA256_SIG_MIN_ASN1_LENGTH (8).
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 4);
    s_proxy_info_ctx ctx = {0};
    buffer_t buf = {.ptr = tlv, .size = len, .offset = 0};
    TEST_ASSERT_FALSE(handle_proxy_info_tlv_payload(&buf, &ctx));
}

void test_lookup_chain_id_mismatch_returns_null(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_TRUE(run_parse_and_verify(tlv, len, &ctx));
    // Wrong chain_id must yield NULL even when address+selector match.
    uint64_t wrong_chain = 137;
    TEST_ASSERT_NULL(get_implem_contract(&wrong_chain, g_proxy_addr, g_selector));
}

void test_lookup_address_mismatch_returns_null(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_TRUE(run_parse_and_verify(tlv, len, &ctx));
    uint8_t wrong_addr[ADDRESS_LENGTH];
    memset(wrong_addr, 0xCC, ADDRESS_LENGTH);
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_implem_contract(&chain, wrong_addr, g_selector));
}

void test_lookup_selector_mismatch_returns_null(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_TRUE(run_parse_and_verify(tlv, len, &ctx));
    static const uint8_t wrong_selector[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    uint64_t chain = 1;
    TEST_ASSERT_NULL(get_implem_contract(&chain, g_proxy_addr, wrong_selector));
}

void test_lookup_without_selector_in_descriptor_ignores_caller_selector(void) {
    // When the registered descriptor has no selector (has_selector=false),
    // the runtime check skips the selector comparison. Any caller selector
    // is then accepted.
    uint8_t tlv[256];
    size_t len = build_tlv(tlv,
                           sizeof(tlv),
                           0x26,
                           0x01,
                           1,
                           g_proxy_addr,
                           g_impl_addr,
                           /*include_selector=*/false,
                           0,
                           16);
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_TRUE(run_parse_and_verify(tlv, len, &ctx));
    static const uint8_t any_selector[CALLDATA_SELECTOR_SIZE] = {0xCA, 0xFE, 0xBA, 0xBE};
    uint64_t chain = 1;
    const uint8_t *impl = get_implem_contract(&chain, g_proxy_addr, any_selector);
    TEST_ASSERT_NOT_NULL(impl);
}

void test_proxy_cleanup_releases_registration(void) {
    uint8_t tlv[256];
    size_t len = build_tlv(tlv, sizeof(tlv), 0x26, 0x01, 1, g_proxy_addr, g_impl_addr, true, 0, 16);
    s_proxy_info_ctx ctx = {0};
    TEST_ASSERT_TRUE(run_parse_and_verify(tlv, len, &ctx));
    uint64_t chain = 1;
    TEST_ASSERT_NOT_NULL(get_implem_contract(&chain, g_proxy_addr, g_selector));

    proxy_cleanup();
    // After cleanup, both lookups go back to NULL.
    TEST_ASSERT_NULL(get_implem_contract(&chain, g_proxy_addr, g_selector));
    TEST_ASSERT_NULL(get_proxy_contract(&chain, g_impl_addr, g_selector));
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
    RUN_TEST(test_happy_path_registers_descriptor);
    RUN_TEST(test_lookups_without_registered_descriptor_return_null);
    RUN_TEST(test_signature_check_failure_rejects);
    RUN_TEST(test_finalize_hash_failure_rejects);
    RUN_TEST(test_invalid_struct_type_rejected);
    RUN_TEST(test_invalid_struct_version_rejected);
    RUN_TEST(test_delegation_type_out_of_range_rejected);
    RUN_TEST(test_signature_too_short_rejected);
    RUN_TEST(test_lookup_chain_id_mismatch_returns_null);
    RUN_TEST(test_lookup_address_mismatch_returns_null);
    RUN_TEST(test_lookup_selector_mismatch_returns_null);
    RUN_TEST(test_lookup_without_selector_in_descriptor_ignores_caller_selector);
    RUN_TEST(test_proxy_cleanup_releases_registration);
    return UNITY_END();
}
