/**
 * @file test_safe_descriptors.c
 * @brief Unit tests for the Gnosis-Safe descriptor backend at
 *        src/features/provide_safe_account/{safe,signer}_descriptor.c.
 *
 * A Safe account is rendered by the device using two paired backend-
 * signed payloads:
 *   - safe_descriptor: identifies the Safe by (address, threshold,
 *     signers_count, role-of-current-user). Signed by the LES_MULTISIG
 *     backend.
 *   - signer_descriptor: lists the actual signer addresses (count must
 *     match SAFE_DESC->signers_count). Also signed by LES_MULTISIG.
 *
 * The two descriptors must arrive in order: safe first, then signer.
 * Both share a CHALLENGE TLV; roll_challenge is called at distinct
 * points to invalidate the captured pair against later replays
 * (CWE-294 mitigation).
 *
 * Coverage drives both files end-to-end through real tlv_apdu /
 * tlv_utils stack with the SDK crypto / signature-verification calls
 * wrapped.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "safe_descriptor.h"
#include "signer_descriptor.h"
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

static int g_roll_challenge_calls = 0;
void roll_challenge(void) {
    g_roll_challenge_calls++;
}

// =============================================================================
// Test data
// =============================================================================

static const uint8_t g_safe_addr[ADDRESS_LENGTH] = {
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};
static const uint8_t g_signer1[ADDRESS_LENGTH] = {
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
};
static const uint8_t g_signer2[ADDRESS_LENGTH] = {
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
};

// =============================================================================
// TLV builders
// =============================================================================

typedef struct {
    uint8_t struct_type;
    uint8_t struct_version;
    const uint8_t *address;
    uint16_t threshold;
    uint16_t signers_count;
    uint8_t role;
    uint8_t sig_len;
    bool omit_address;
    bool omit_threshold;
} s_safe_opts;

static size_t build_safe_tlv(uint8_t *out, size_t out_size, s_safe_opts opts) {
    size_t off = 0;
    out[off++] = 0x01;
    out[off++] = 0x01;
    out[off++] = opts.struct_type;
    out[off++] = 0x02;
    out[off++] = 0x01;
    out[off++] = opts.struct_version;
    out[off++] = 0x12;
    out[off++] = 0x04;
    out[off++] = 0xDE;
    out[off++] = 0xAD;
    out[off++] = 0xBE;
    out[off++] = 0xEF;
    if (!opts.omit_address) {
        out[off++] = 0x22;
        out[off++] = ADDRESS_LENGTH;
        memcpy(out + off, opts.address, ADDRESS_LENGTH);
        off += ADDRESS_LENGTH;
    }
    // tag 0xa0 = THRESHOLD ≥ 0x80 needs DER long-form (0x81 0xa0)
    if (!opts.omit_threshold) {
        out[off++] = 0x81;
        out[off++] = 0xa0;
        out[off++] = 0x02;
        out[off++] = (uint8_t) (opts.threshold >> 8);
        out[off++] = (uint8_t) (opts.threshold & 0xFF);
    }
    // 0xa1 SIGNERS_COUNT
    out[off++] = 0x81;
    out[off++] = 0xa1;
    out[off++] = 0x02;
    out[off++] = (uint8_t) (opts.signers_count >> 8);
    out[off++] = (uint8_t) (opts.signers_count & 0xFF);
    // 0xa2 ROLE
    out[off++] = 0x81;
    out[off++] = 0xa2;
    out[off++] = 0x01;
    out[off++] = opts.role;
    // 0x15 SIGNATURE
    out[off++] = 0x15;
    out[off++] = opts.sig_len;
    memset(out + off, 0x42, opts.sig_len);
    off += opts.sig_len;
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

typedef struct {
    uint8_t struct_type;
    uint8_t struct_version;
    const uint8_t *addresses[4];
    uint8_t address_count;
    uint8_t sig_len;
} s_signer_opts;

static size_t build_signer_tlv(uint8_t *out, size_t out_size, s_signer_opts opts) {
    size_t off = 0;
    out[off++] = 0x01;
    out[off++] = 0x01;
    out[off++] = opts.struct_type;
    out[off++] = 0x02;
    out[off++] = 0x01;
    out[off++] = opts.struct_version;
    out[off++] = 0x12;
    out[off++] = 0x04;
    out[off++] = 0xDE;
    out[off++] = 0xAD;
    out[off++] = 0xBE;
    out[off++] = 0xEF;
    for (int i = 0; i < opts.address_count; ++i) {
        out[off++] = 0x22;
        out[off++] = ADDRESS_LENGTH;
        memcpy(out + off, opts.addresses[i], ADDRESS_LENGTH);
        off += ADDRESS_LENGTH;
    }
    out[off++] = 0x15;
    out[off++] = opts.sig_len;
    memset(out + off, 0x42, opts.sig_len);
    off += opts.sig_len;
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    clear_safe_descriptor();
    clear_signer_descriptor();
    s_sig_check_ret = true;
    s_finalize_hash_ret = true;
    g_roll_challenge_calls = 0;
}

static bool run_safe(const uint8_t *bytes, size_t len) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = len, .offset = 0};
    return handle_safe_tlv_payload(&buf);
}

static bool run_signer(const uint8_t *bytes, size_t len) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = len, .offset = 0};
    return handle_signer_tlv_payload(&buf);
}

// =============================================================================
// safe_descriptor tests
// =============================================================================

void test_safe_happy_path_registers(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run_safe(tlv, len));
    TEST_ASSERT_NOT_NULL(SAFE_DESC);
    TEST_ASSERT_EQUAL(SAFE_DESC->threshold, 2);
    TEST_ASSERT_EQUAL(SAFE_DESC->signers_count, 3);
    TEST_ASSERT_EQUAL(SAFE_DESC->role, ROLE_SIGNER);
    // Happy path does NOT roll the challenge — the signer descriptor
    // that follows is signed against the same challenge.
    TEST_ASSERT_EQUAL(g_roll_challenge_calls, 0);
}

void test_safe_invalid_struct_type_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0xFF,  // not TYPE_LESM_ACCOUNT_INFO
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
    TEST_ASSERT_NULL(SAFE_DESC);
    // Failure path rolls the challenge (replay defense).
    TEST_ASSERT_EQUAL(g_roll_challenge_calls, 1);
}

void test_safe_invalid_struct_version_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x05,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

void test_safe_address_all_zeros_rejected(void) {
    static const uint8_t zero_addr[ADDRESS_LENGTH] = {0};
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = zero_addr,
                        .threshold = 2,
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

void test_safe_threshold_zero_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 0,  // below min 1
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

void test_safe_threshold_above_max_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 101,  // above MAX_THRESHOLD=100
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

void test_safe_signers_count_zero_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = 0,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

void test_safe_role_out_of_range_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = 3,
                        .role = 0x7F,  // outside [0, ROLE_MAX=2]
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

void test_safe_signature_check_failure_rejects(void) {
    s_sig_check_ret = false;
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
    TEST_ASSERT_NULL(SAFE_DESC);
}

void test_safe_signature_too_short_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .sig_len = 4};  // < MIN=8
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

void test_safe_missing_threshold_rejected(void) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .signers_count = 3,
                        .role = ROLE_SIGNER,
                        .omit_threshold = true,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_safe(tlv, len));
}

// =============================================================================
// signer_descriptor tests — require a registered safe_descriptor.
// =============================================================================

static void register_safe_for_signer_tests(uint16_t signers_count) {
    uint8_t tlv[200];
    s_safe_opts opts = {.struct_type = 0x27,
                        .struct_version = 0x01,
                        .address = g_safe_addr,
                        .threshold = 2,
                        .signers_count = signers_count,
                        .role = ROLE_SIGNER,
                        .sig_len = 16};
    size_t len = build_safe_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run_safe(tlv, len));
}

void test_signer_without_safe_rejected(void) {
    // No prior safe_descriptor — SAFE_DESC is NULL.
    TEST_ASSERT_NULL(SAFE_DESC);
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_signer(tlv, len));
}

void test_signer_duplicate_rejected(void) {
    register_safe_for_signer_tests(2);
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run_signer(tlv, len));
    // Second invocation with SIGNER_DESC.data already set must reject.
    TEST_ASSERT_FALSE(run_signer(tlv, len));
}

void test_signer_happy_path_registers(void) {
    register_safe_for_signer_tests(2);
    g_roll_challenge_calls = 0;  // ignore safe-registration
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run_signer(tlv, len));
    TEST_ASSERT_TRUE(SIGNER_DESC.is_valid);
    // Signer-descriptor consumption always rolls the challenge
    // (CWE-294 mitigation, success and failure alike).
    TEST_ASSERT_EQUAL(g_roll_challenge_calls, 1);
}

void test_signer_invalid_struct_type_rejected(void) {
    register_safe_for_signer_tests(2);
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0xFF,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_signer(tlv, len));
}

void test_signer_address_all_zeros_rejected(void) {
    register_safe_for_signer_tests(2);
    static const uint8_t zero_addr[ADDRESS_LENGTH] = {0};
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, zero_addr},  // 2nd is zero
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_signer(tlv, len));
}

void test_signer_too_few_addresses_rejected(void) {
    register_safe_for_signer_tests(3);  // SAFE expects 3
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},  // only 2
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_signer(tlv, len));
}

void test_signer_too_many_addresses_rejected(void) {
    register_safe_for_signer_tests(1);  // SAFE expects 1
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},
                          .address_count = 2,  // but we send 2
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_signer(tlv, len));
}

void test_signer_signature_check_failure_rejects(void) {
    register_safe_for_signer_tests(2);
    s_sig_check_ret = false;
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(run_signer(tlv, len));
}

void test_clear_safe_and_signer_releases(void) {
    register_safe_for_signer_tests(2);
    uint8_t tlv[200];
    s_signer_opts opts = {.struct_type = 0x0A,
                          .struct_version = 0x01,
                          .addresses = {g_signer1, g_signer2},
                          .address_count = 2,
                          .sig_len = 16};
    size_t len = build_signer_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(run_signer(tlv, len));
    TEST_ASSERT_NOT_NULL(SAFE_DESC);
    TEST_ASSERT_NOT_NULL(SIGNER_DESC.data);
    clear_signer_descriptor();
    TEST_ASSERT_NULL(SIGNER_DESC.data);
    clear_safe_descriptor();
    TEST_ASSERT_NULL(SAFE_DESC);
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
    RUN_TEST(test_safe_happy_path_registers);
    RUN_TEST(test_safe_invalid_struct_type_rejected);
    RUN_TEST(test_safe_invalid_struct_version_rejected);
    RUN_TEST(test_safe_address_all_zeros_rejected);
    RUN_TEST(test_safe_threshold_zero_rejected);
    RUN_TEST(test_safe_threshold_above_max_rejected);
    RUN_TEST(test_safe_signers_count_zero_rejected);
    RUN_TEST(test_safe_role_out_of_range_rejected);
    RUN_TEST(test_safe_signature_check_failure_rejects);
    RUN_TEST(test_safe_signature_too_short_rejected);
    RUN_TEST(test_safe_missing_threshold_rejected);
    RUN_TEST(test_signer_without_safe_rejected);
    RUN_TEST(test_signer_duplicate_rejected);
    RUN_TEST(test_signer_happy_path_registers);
    RUN_TEST(test_signer_invalid_struct_type_rejected);
    RUN_TEST(test_signer_address_all_zeros_rejected);
    RUN_TEST(test_signer_too_few_addresses_rejected);
    RUN_TEST(test_signer_too_many_addresses_rejected);
    RUN_TEST(test_signer_signature_check_failure_rejects);
    RUN_TEST(test_clear_safe_and_signer_releases);
    return UNITY_END();
}
