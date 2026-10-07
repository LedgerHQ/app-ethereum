/**
 * @file test_tx_info.c
 * @brief Unit tests for the top-level GCS tx_info struct at
 *        src/features/generic_tx_parser/gtp_tx_info.c.
 *
 * tx_info carries the metadata the device shows for a GCS-driven
 * transaction (operation type, creator name / url, contract name,
 * deploy date) plus the (chain_id, contract_addr, selector,
 * fields_hash) tuple used to bind the signed payload to a concrete
 * call. The module exposes:
 *   - per-tag TLV handlers (covered through handle_tx_info_struct),
 *   - verify_tx_info_struct: enforces version + mandatory tag presence
 *     + signature validity,
 *   - seven getters that all share the same (NULL ? NULL : (empty ?
 *     NULL : field)) shape.
 *
 * Tests focus on:
 *   - the getters' NULL / empty short-circuits,
 *   - happy-path TLV parsing populating every field correctly,
 *   - handle_selector: the security-critical match against the parked
 *     calldata selector (CWE-345-style binding),
 *   - handle_fields_hash / handle_deploy_date size guards,
 *   - verify_tx_info_struct: rejection on missing version, unsupported
 *     version, signature failure, and the happy path.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_tx_info.h"
#include "shared_context.h"
#include "tx_ctx.h"
#include "calldata.h"
#include "Mockpublic_keys.h"
#include "Mockhash_bytes.h"

// =============================================================================
// Wrapped collaborators
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

static size_t g_tx_ctx_count = 0;
size_t get_tx_ctx_count(void) {
    return g_tx_ctx_count;
}

static const uint8_t *g_selector_ret = NULL;
const uint8_t *calldata_get_selector(const s_calldata *calldata) {
    (void) calldata;
    return g_selector_ret;
}

// =============================================================================
// Fixtures
// =============================================================================

static void reset(void) {
    s_finalize_hash_ret = true;
    s_sig_check_ret = true;
    g_tx_ctx_count = 1;  // Skip the selector-match by default; tests that
                         // want to exercise the match set it to 0.
    g_selector_ret = NULL;
}

// =============================================================================
// Getters — NULL guard + empty-string guard
// =============================================================================

void test_getters_null_tx_info_returns_null(void) {
    TEST_ASSERT_NULL(get_operation_type(NULL));
    TEST_ASSERT_NULL(get_creator_name(NULL));
    TEST_ASSERT_NULL(get_creator_legal_name(NULL));
    TEST_ASSERT_NULL(get_creator_url(NULL));
    TEST_ASSERT_NULL(get_contract_name(NULL));
    TEST_ASSERT_NULL(get_deploy_date(NULL));
    TEST_ASSERT_NULL(get_contract_addr(NULL));
}

void test_getters_empty_string_returns_null(void) {
    s_tx_info info = {0};
    TEST_ASSERT_NULL(get_operation_type(&info));
    TEST_ASSERT_NULL(get_creator_name(&info));
    TEST_ASSERT_NULL(get_creator_legal_name(&info));
    TEST_ASSERT_NULL(get_creator_url(&info));
    TEST_ASSERT_NULL(get_contract_name(&info));
    TEST_ASSERT_NULL(get_deploy_date(&info));
    // get_contract_addr only NULL-guards on tx_info, not on the address
    // bytes — it returns the (possibly zero) address as-is.
    TEST_ASSERT_EQUAL_PTR(get_contract_addr(&info), info.contract_addr);
}

void test_getters_return_populated_fields(void) {
    s_tx_info info = {0};
    strlcpy(info.operation_type, "Approve", sizeof(info.operation_type));
    strlcpy(info.creator_name, "Aave", sizeof(info.creator_name));
    strlcpy(info.creator_legal_name, "Aave DAO", sizeof(info.creator_legal_name));
    strlcpy(info.creator_url, "aave.com", sizeof(info.creator_url));
    strlcpy(info.contract_name, "Pool", sizeof(info.contract_name));
    strlcpy(info.deploy_date, "2024-01-01", sizeof(info.deploy_date));

    TEST_ASSERT_EQUAL_STRING(get_operation_type(&info), "Approve");
    TEST_ASSERT_EQUAL_STRING(get_creator_name(&info), "Aave");
    TEST_ASSERT_EQUAL_STRING(get_creator_legal_name(&info), "Aave DAO");
    TEST_ASSERT_EQUAL_STRING(get_creator_url(&info), "aave.com");
    TEST_ASSERT_EQUAL_STRING(get_contract_name(&info), "Pool");
    TEST_ASSERT_EQUAL_STRING(get_deploy_date(&info), "2024-01-01");
}

// =============================================================================
// handle_tx_info_struct — happy path
// =============================================================================
//
// TLV byte layout: tag(1B) | length(1B for <128) | value.

static bool run_tlv(const uint8_t *bytes, size_t size, s_tx_info_ctx *ctx) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    return handle_tx_info_struct(&buf, ctx);
}

void test_tlv_happy_path_populates_fields(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};

    // Skip the selector match path
    g_tx_ctx_count = 1;

    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x01,
        0x02,
        0x00,
        0x01,  // CHAIN_ID = 1
        // CONTRACT_ADDR (20 bytes)
        0x02,
        0x14,
        0xAA,
        0xBB,
        0xCC,
        0xDD,
        0xEE,
        0xFF,
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x99,
        0x00,
        0xAA,
        0xBB,
        0xCC,
        0xDD,
        // SELECTOR (4 bytes)
        0x03,
        0x04,
        0xDE,
        0xAD,
        0xBE,
        0xEF,
        // FIELDS_HASH (32 bytes) — only first 4 set for brevity, helper
        // left-pads with zeros via buf_shrink_expand.
        0x04,
        0x04,
        0x01,
        0x02,
        0x03,
        0x04,
        // OPERATION_TYPE
        0x05,
        0x07,
        'A',
        'p',
        'p',
        'r',
        'o',
        'v',
        'e',
        // CREATOR_NAME
        0x06,
        0x04,
        'A',
        'a',
        'v',
        'e',
        // CONTRACT_NAME
        0x09,
        0x04,
        'P',
        'o',
        'o',
        'l',
        // DEPLOY_DATE (4-byte BE timestamp = 1704067200 = 2024-01-01)
        0x0A,
        0x04,
        0x65,
        0x92,
        0x00,
        0x80,
        // SIGNATURE — any bytes, length is captured for the verify step
        0x81,
        0xFF,
        0x03,
        0xAA,
        0xBB,
        0xCC,  // DER long-form for tag 0xFF
    };
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_EQUAL(info.version, 1);
    TEST_ASSERT_EQUAL(info.chain_id, 1);
    static const uint8_t expected_addr[ADDRESS_LENGTH] = {
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD,
    };
    TEST_ASSERT_EQUAL_MEMORY(info.contract_addr, expected_addr, ADDRESS_LENGTH);
    static const uint8_t expected_selector[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL_MEMORY(info.selector, expected_selector, sizeof(expected_selector));
    TEST_ASSERT_EQUAL_STRING(info.operation_type, "Approve");
    TEST_ASSERT_EQUAL_STRING(info.creator_name, "Aave");
    TEST_ASSERT_EQUAL_STRING(info.contract_name, "Pool");
    TEST_ASSERT_EQUAL_STRING(info.deploy_date, "2024-01-01");
    TEST_ASSERT_EQUAL(info.signature_len, 3);
}

// =============================================================================
// handle_selector — security-critical match against parked calldata
// =============================================================================

void test_tlv_selector_match_against_parked_calldata(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};

    // Force the match path: ctx_count==0 → handle_selector must compare
    // against calldata_get_selector(g_parked_calldata).
    g_tx_ctx_count = 0;
    static const uint8_t parked[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    g_selector_ret = parked;

    const uint8_t bytes[] = {
        0x03,
        0x04,
        0xDE,
        0xAD,
        0xBE,
        0xEF,  // matching selector
    };
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_EQUAL_MEMORY(info.selector, parked, sizeof(parked));
}

void test_tlv_selector_mismatch_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 0;
    static const uint8_t parked[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    g_selector_ret = parked;

    const uint8_t bytes[] = {
        0x03,
        0x04,
        0xCA,
        0xFE,
        0xBA,
        0xBE,  // does NOT match parked
    };
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &ctx));
}

void test_tlv_selector_no_parked_calldata_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 0;
    g_selector_ret = NULL;  // no parked calldata at all

    const uint8_t bytes[] = {
        0x03,
        0x04,
        0xDE,
        0xAD,
        0xBE,
        0xEF,
    };
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &ctx));
}

// =============================================================================
// Size guards
// =============================================================================

void test_tlv_fields_hash_oversized_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;

    // FIELDS_HASH > 32 bytes is rejected.
    uint8_t bytes[2 + 33];
    bytes[0] = 0x04;
    bytes[1] = 33;
    memset(&bytes[2], 0xAA, 33);
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &ctx));
}

void test_tlv_deploy_date_oversized_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;

    // DEPLOY_DATE > 4 bytes is rejected.
    uint8_t bytes[2 + 5];
    bytes[0] = 0x0A;
    bytes[1] = 5;
    memset(&bytes[2], 0, 5);
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &ctx));
}

void test_tlv_selector_oversize_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;

    // SELECTOR > CALLDATA_SELECTOR_SIZE (4) bytes — tlv_get_hash's max_size
    // guard inside handle_selector must reject before we ever look at the
    // parked-calldata cross-check.
    uint8_t bytes[2 + 5];
    bytes[0] = 0x03;
    bytes[1] = 5;
    memset(&bytes[2], 0xAA, 5);
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &ctx));
}

void test_tlv_signature_oversize_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;

    // SIGNATURE > sizeof(tx_info->signature) (CX_ECDSA_SHA256_SIG_MAX_ASN1_LENGTH=72)
    // must be rejected — otherwise the memcpy on line 133 would overflow
    // the on-stack-or-heap signature buffer. This is the critical
    // size-cap on attacker-controlled signed-descriptor input.
    // Tag 0xFF needs DER long-form encoding (0x81 0xFF).
    uint8_t bytes[3 + 73];
    bytes[0] = 0x81;
    bytes[1] = 0xFF;
    bytes[2] = 73;  // length, < 128 so single byte
    memset(&bytes[3], 0x42, 73);
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_EQUAL(info.signature_len, 0);  // nothing copied through.
}

// Exercise the under-tested optional metadata tags (creator_legal_name,
// creator_url, contract_name, creator_name, deploy_date) so the
// per-tag string-truncation helpers are pinned. These tags drive
// what's displayed in the operation summary, so even though they're
// non-cryptographic, an unexercised truncation path is a UX-trust
// surface.
void test_tlv_optional_metadata_tags_populated(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;

    const uint8_t bytes[] = {
        0x06, 0x07, 'A',  'a',  ' ',  'I',  'n', 'c', '.',                 // CREATOR_NAME
        0x07, 0x0A, 'A',  'a',  ' ',  'I',  'n', 'c', '.', ' ', 'L', 'P',  // CREATOR_LEGAL_NAME
        0x08, 0x10, 'h',  't',  't',  'p',  's', ':', '/', '/', 'a', 'a',
        '.',  'i',  'o',  '/',  '?',  'q',        // CREATOR_URL
        0x09, 0x05, 'P',  'o',  'o',  'l',  'A',  // CONTRACT_NAME
        0x0A, 0x04, 0x65, 0x92, 0x00, 0x80,       // DEPLOY_DATE (2024-01-01)
    };
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_EQUAL_STRING(info.creator_name, "Aa Inc.");
    TEST_ASSERT_EQUAL_STRING(info.creator_legal_name, "Aa Inc. LP");
    TEST_ASSERT_EQUAL_STRING(info.creator_url, "https://aa.io/?q");
    TEST_ASSERT_EQUAL_STRING(info.contract_name, "PoolA");
    TEST_ASSERT_EQUAL_STRING(info.deploy_date, "2024-01-01");
}

// delete_tx_info is the dual of APP_MEM_CALLOC in cmd_tx_info — must
// not crash on a heap-allocated node (mocks use real malloc/free).
void test_delete_tx_info_frees_node(void) {
    s_tx_info *node = calloc(1, sizeof(*node));
    TEST_ASSERT_NOT_NULL(node);
    delete_tx_info(node);  // no crash, no leak.
}

// =============================================================================
// verify_tx_info_struct
// =============================================================================

// Build a fully-valid TLV that hits every required tag for version 1.
static const uint8_t g_valid_v1_tlv[] = {
    0x00, 0x01, 0x01,  // VERSION = 1
    0x01, 0x02, 0x00, 0x01, 0x02, 0x14, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
    0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x03, 0x04,
    0xDE, 0xAD, 0xBE, 0xEF, 0x04, 0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x07, 'A',  'p',
    'p',  'r',  'o',  'v',  'e',  0x81, 0xFF, 0x03, 0xAA, 0xBB, 0xCC,  // SIGNATURE (DER long-form
                                                                       // for tag 0xFF)
};

void test_verify_missing_version_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    // Empty TLV → no tag set → TAG_VERSION not received.
    TEST_ASSERT_TRUE(run_tlv(NULL, 0, &ctx));
    TEST_ASSERT_FALSE(verify_tx_info_struct(&ctx));
}

// VERSION = 1 but other required tags (CHAIN_ID, CONTRACT_ADDR,
// SELECTOR, FIELDS_HASH, OPERATION_TYPE, SIGNATURE) absent. The switch
// in verify_tx_info_struct routes to case 1 which fails the
// multi-tag presence check — separate code path from "missing VERSION".
void test_verify_missing_required_field_with_version_present(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;
    const uint8_t bytes[] = {0x00, 0x01, 0x01};  // VERSION = 1, nothing else
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_FALSE(verify_tx_info_struct(&ctx));
}

void test_verify_unsupported_version_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;
    const uint8_t bytes[] = {0x00, 0x01, 0x99};  // VERSION = 0x99 (not 1)
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &ctx));
    TEST_ASSERT_FALSE(verify_tx_info_struct(&ctx));
}

void test_verify_happy_path(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;
    TEST_ASSERT_TRUE(run_tlv(g_valid_v1_tlv, sizeof(g_valid_v1_tlv), &ctx));
    TEST_ASSERT_TRUE(verify_tx_info_struct(&ctx));
}

void test_verify_signature_check_failure_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;
    TEST_ASSERT_TRUE(run_tlv(g_valid_v1_tlv, sizeof(g_valid_v1_tlv), &ctx));
    s_sig_check_ret = false;
    TEST_ASSERT_FALSE(verify_tx_info_struct(&ctx));
}

void test_verify_finalize_hash_failure_rejected(void) {
    s_tx_info info = {0};
    s_tx_info_ctx ctx = {.tx_info = &info};
    g_tx_ctx_count = 1;
    TEST_ASSERT_TRUE(run_tlv(g_valid_v1_tlv, sizeof(g_valid_v1_tlv), &ctx));
    s_finalize_hash_ret = false;
    TEST_ASSERT_FALSE(verify_tx_info_struct(&ctx));
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
    RUN_TEST(test_getters_null_tx_info_returns_null);
    RUN_TEST(test_getters_empty_string_returns_null);
    RUN_TEST(test_getters_return_populated_fields);
    RUN_TEST(test_tlv_happy_path_populates_fields);
    RUN_TEST(test_tlv_selector_match_against_parked_calldata);
    RUN_TEST(test_tlv_selector_mismatch_rejected);
    RUN_TEST(test_tlv_selector_no_parked_calldata_rejected);
    RUN_TEST(test_tlv_fields_hash_oversized_rejected);
    RUN_TEST(test_tlv_deploy_date_oversized_rejected);
    RUN_TEST(test_tlv_selector_oversize_rejected);
    RUN_TEST(test_tlv_signature_oversize_rejected);
    RUN_TEST(test_tlv_optional_metadata_tags_populated);
    RUN_TEST(test_delete_tx_info_frees_node);
    RUN_TEST(test_verify_missing_version_rejected);
    RUN_TEST(test_verify_missing_required_field_with_version_present);
    RUN_TEST(test_verify_unsupported_version_rejected);
    RUN_TEST(test_verify_happy_path);
    RUN_TEST(test_verify_signature_check_failure_rejected);
    RUN_TEST(test_verify_finalize_hash_failure_rejected);
    return UNITY_END();
}
