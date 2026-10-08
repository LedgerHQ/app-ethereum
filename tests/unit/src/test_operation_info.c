/**
 * @file test_operation_info.c
 * @brief Unit tests for the operation info struct at
 *        src/features/gcs/operation_info.c, shared by TRANSACTION_INFO (GCS) and
 *        EIP712_MESSAGE_INFO.
 *
 * operation_info carries the metadata the device shows for a clear-signed
 * operation (operation type, creator name / url, contract name,
 * deploy date) plus what binds the signed payload to a concrete call (chain_id,
 * contract_addr, selector) or message (domain separator, primary type hash), and the
 * fields_hash. The module exposes new_operation_info, which parses the TLV
 * payload, enforces the version and the mandatory tags of the context, and checks the
 * signature; and getters that all share the same (NULL ? NULL : (empty ? NULL : field)) shape.
 *
 * Everything but the getters goes through new_operation_info, from payloads that are valid
 * but for the one thing a test changes. Tests focus on:
 *   - the getters' NULL / empty short-circuits,
 *   - happy-path TLV parsing populating every field correctly,
 *   - the exact size of a selector or a hash (the binding to the parked calldata is the
 *     command's, see test_cmd_field_tx_info),
 *   - the exact size of fields_hash, and the size guards on deploy_date and signature,
 *   - the tags of each context, mandatory or refused, the supported version, the signature
 *     check.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "operation_info.h"
#include "shared_context.h"
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
// the digest finalize_hash hands out, tests set it to inject the bytes to compare against
static uint8_t s_finalize_hash_out[INT256_LENGTH];
static bool finalize_hash_stub(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len, int num_calls) {
    (void) hash_ctx;
    (void) num_calls;
    memcpy(out, s_finalize_hash_out, MIN(out_len, sizeof(s_finalize_hash_out)));
    return s_finalize_hash_ret;
}

// =============================================================================
// Fixtures
// =============================================================================

static void reset(void) {
    s_finalize_hash_ret = true;
    memset(s_finalize_hash_out, 0, sizeof(s_finalize_hash_out));
    s_sig_check_ret = true;
}

// =============================================================================
// Payload builder
// =============================================================================
//
// TLV byte layout: tag | length | value. Tags from 0x80 up carry a 0x81 prefix (DER long form),
// lengths stay below 0x80 here.

enum {
    WIRE_TAG_VERSION = 0x00,
    WIRE_TAG_CHAIN_ID = 0x01,
    WIRE_TAG_CONTRACT_ADDR = 0x02,
    WIRE_TAG_SELECTOR = 0x03,
    WIRE_TAG_FIELDS_HASH = 0x04,
    WIRE_TAG_OPERATION_TYPE = 0x05,
    WIRE_TAG_CREATOR_NAME = 0x06,
    WIRE_TAG_CREATOR_LEGAL_NAME = 0x07,
    WIRE_TAG_CREATOR_URL = 0x08,
    WIRE_TAG_CONTRACT_NAME = 0x09,
    WIRE_TAG_DEPLOY_DATE = 0x0A,
    WIRE_TAG_DOMAIN_SEPARATOR = 0x0B,
    WIRE_TAG_PRIMARY_TYPE_HASH = 0x0C,
    WIRE_TAG_SIGNATURE = 0xFF,
};

// Leave a tag out of valid(): the tags in use all differ modulo 32
#define OMIT(tag) (1U << ((tag) % 32))

typedef struct {
    uint8_t bytes[256];
    size_t size;
} payload_t;

static void put(payload_t *p, uint8_t tag, const void *value, size_t len) {
    TEST_ASSERT_TRUE(len < 0x80);
    TEST_ASSERT_TRUE(p->size + 3 + len <= sizeof(p->bytes));
    if (tag >= 0x80) {
        p->bytes[p->size++] = 0x81;
    }
    p->bytes[p->size++] = tag;
    p->bytes[p->size++] = (uint8_t) len;
    memcpy(&p->bytes[p->size], value, len);
    p->size += len;
}

static void put_str(payload_t *p, uint8_t tag, const char *value) {
    put(p, tag, value, strlen(value));
}

#define TYPE_HASH_BYTES                                                                           \
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,     \
        0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, \
        0xCE, 0xCF

static const uint8_t k_version[] = {0x01};
static const uint8_t k_chain_id[] = {0x00, 0x01};
static const uint8_t k_contract[ADDRESS_LENGTH] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11,
                                                   0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                                   0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD};
static const uint8_t k_selector[CALLDATA_SELECTOR_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
static const uint8_t k_domain_separator[INT256_LENGTH] = {0xD0, 0xD1, 0xD2, 0xD3};
static const uint8_t k_type_hash[INT256_LENGTH] = {TYPE_HASH_BYTES};
static const uint8_t k_fields_hash[INT256_LENGTH] = {0x01, 0x02, 0x03, 0x04};
static const uint8_t k_signature[] = {0xAA, 0xBB, 0xCC};

// A payload with every tag the context requires, but the ones in `omit`. A TRANSACTION_INFO
// requires the chain and the contract, an EIP712_MESSAGE_INFO leaves them out.
static payload_t valid(e_operation_info_context context, uint32_t omit) {
    payload_t p = {0};

#define PUT_UNLESS_OMITTED(tag, value, len) \
    do {                                    \
        if (!(omit & OMIT(tag))) {          \
            put(&p, tag, value, len);       \
        }                                   \
    } while (0)

    PUT_UNLESS_OMITTED(WIRE_TAG_VERSION, k_version, sizeof(k_version));
    if (context == OPERATION_INFO_CTX_SIGN_TX) {
        PUT_UNLESS_OMITTED(WIRE_TAG_CHAIN_ID, k_chain_id, sizeof(k_chain_id));
        PUT_UNLESS_OMITTED(WIRE_TAG_CONTRACT_ADDR, k_contract, sizeof(k_contract));
        PUT_UNLESS_OMITTED(WIRE_TAG_SELECTOR, k_selector, sizeof(k_selector));
    } else {
        PUT_UNLESS_OMITTED(WIRE_TAG_DOMAIN_SEPARATOR,
                           k_domain_separator,
                           sizeof(k_domain_separator));
        PUT_UNLESS_OMITTED(WIRE_TAG_PRIMARY_TYPE_HASH, k_type_hash, sizeof(k_type_hash));
    }
    PUT_UNLESS_OMITTED(WIRE_TAG_FIELDS_HASH, k_fields_hash, sizeof(k_fields_hash));
    PUT_UNLESS_OMITTED(WIRE_TAG_OPERATION_TYPE, "Approve", strlen("Approve"));
    PUT_UNLESS_OMITTED(WIRE_TAG_SIGNATURE, k_signature, sizeof(k_signature));
#undef PUT_UNLESS_OMITTED
    return p;
}

static s_operation_info *parse(const payload_t *p, e_operation_info_context context) {
    buffer_t buf = {.ptr = (uint8_t *) p->bytes, .size = p->size, .offset = 0};

    return new_operation_info(&buf, context);
}

// Parse a payload the test expects to be accepted
static s_operation_info *parse_accepted(const payload_t *p, e_operation_info_context context) {
    s_operation_info *info = parse(p, context);

    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL(context, info->context);
    return info;
}

static void assert_rejected(const payload_t *p, e_operation_info_context context) {
    TEST_ASSERT_NULL(parse(p, context));
}

// =============================================================================
// Getters — NULL guard + empty-string guard
// =============================================================================

void test_getters_null_operation_info_returns_null(void) {
    TEST_ASSERT_NULL(get_operation_type(NULL));
    TEST_ASSERT_NULL(get_creator_name(NULL));
    TEST_ASSERT_NULL(get_creator_legal_name(NULL));
    TEST_ASSERT_NULL(get_creator_url(NULL));
    TEST_ASSERT_NULL(get_contract_name(NULL));
    TEST_ASSERT_NULL(get_deploy_date(NULL));
}

void test_getters_empty_string_returns_null(void) {
    s_operation_info info = {0};
    TEST_ASSERT_NULL(get_operation_type(&info));
    TEST_ASSERT_NULL(get_creator_name(&info));
    TEST_ASSERT_NULL(get_creator_legal_name(&info));
    TEST_ASSERT_NULL(get_creator_url(&info));
    TEST_ASSERT_NULL(get_contract_name(&info));
    TEST_ASSERT_NULL(get_deploy_date(&info));
}

void test_getters_return_populated_fields(void) {
    s_operation_info info = {0};
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

// delete_operation_info is the dual of the APP_MEM_CALLOC of new_operation_info — must
// not crash on a heap-allocated node (mocks use real malloc/free).
void test_delete_operation_info_frees_node(void) {
    s_operation_info *node = calloc(1, sizeof(*node));
    TEST_ASSERT_NOT_NULL(node);
    delete_operation_info(node);  // no crash, no leak.
}

// =============================================================================
// TRANSACTION_INFO: happy path
// =============================================================================

void test_parse_sign_tx_populates_required_fields(void) {
    payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, 0);
    s_operation_info *info = parse_accepted(&p, OPERATION_INFO_CTX_SIGN_TX);

    TEST_ASSERT_EQUAL(1, info->version);
    TEST_ASSERT_EQUAL(1, info->chain_id);
    TEST_ASSERT_EQUAL_MEMORY(k_contract, info->contract_addr, sizeof(k_contract));
    TEST_ASSERT_EQUAL_PTR(get_selector(info), info->selector);
    TEST_ASSERT_EQUAL_MEMORY(k_selector, info->selector, sizeof(k_selector));
    TEST_ASSERT_EQUAL_STRING("Approve", info->operation_type);
    TEST_ASSERT_EQUAL(sizeof(k_signature), info->signature_len);
    delete_operation_info(info);
}

// Exercise the under-tested optional metadata tags (creator_legal_name,
// creator_url, contract_name, creator_name, deploy_date) so the
// per-tag string-truncation helpers are pinned. These tags drive
// what's displayed in the operation summary, so even though they're
// non-cryptographic, an unexercised truncation path is a UX-trust
// surface.
void test_parse_optional_metadata_tags_populated(void) {
    static const uint8_t deploy_date[] = {0x65, 0x92, 0x00, 0x80};  // 2024-01-01
    payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, 0);
    s_operation_info *info;

    put_str(&p, WIRE_TAG_CREATOR_NAME, "Aa Inc.");
    put_str(&p, WIRE_TAG_CREATOR_LEGAL_NAME, "Aa Inc. LP");
    put_str(&p, WIRE_TAG_CREATOR_URL, "https://aa.io/?q");
    put_str(&p, WIRE_TAG_CONTRACT_NAME, "PoolA");
    put(&p, WIRE_TAG_DEPLOY_DATE, deploy_date, sizeof(deploy_date));
    info = parse_accepted(&p, OPERATION_INFO_CTX_SIGN_TX);

    TEST_ASSERT_EQUAL_STRING("Aa Inc.", info->creator_name);
    TEST_ASSERT_EQUAL_STRING("Aa Inc. LP", info->creator_legal_name);
    TEST_ASSERT_EQUAL_STRING("https://aa.io/?q", info->creator_url);
    TEST_ASSERT_EQUAL_STRING("PoolA", info->contract_name);
    TEST_ASSERT_EQUAL_STRING("2024-01-01", info->deploy_date);
    delete_operation_info(info);
}

// =============================================================================
// Size guards
// =============================================================================

void test_parse_selector_wrong_size_rejected(void) {
    const size_t sizes[] = {CALLDATA_SELECTOR_SIZE - 1, CALLDATA_SELECTOR_SIZE + 1};
    uint8_t selector[CALLDATA_SELECTOR_SIZE + 1];

    memset(selector, 0xAA, sizeof(selector));
    for (size_t i = 0; i < ARRAYLEN(sizes); i++) {
        payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, OMIT(WIRE_TAG_SELECTOR));

        put(&p, WIRE_TAG_SELECTOR, selector, sizes[i]);
        assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
    }
}

void test_parse_fields_hash_of_another_size_rejected(void) {
    // a hash is not padded: one byte short, as one byte over, is no hash
    static const size_t sizes[] = {INT256_LENGTH - 1, INT256_LENGTH + 1};
    uint8_t fields_hash[INT256_LENGTH + 1];

    memset(fields_hash, 0xAA, sizeof(fields_hash));
    for (size_t i = 0; i < ARRAYLEN(sizes); i++) {
        payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, OMIT(WIRE_TAG_FIELDS_HASH));

        put(&p, WIRE_TAG_FIELDS_HASH, fields_hash, sizes[i]);
        assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
    }
}

void test_parse_deploy_date_oversized_rejected(void) {
    static const uint8_t deploy_date[5] = {0};  // a timestamp is 4 bytes
    payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, 0);

    put(&p, WIRE_TAG_DEPLOY_DATE, deploy_date, sizeof(deploy_date));
    assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
}

// A SIGNATURE larger than sizeof(info->signature) (CX_ECDSA_SHA256_SIG_MAX_ASN1_LENGTH=72) must
// be rejected, or the copy would overflow the signature buffer. This is the critical size cap on
// attacker-controlled signed-descriptor input.
void test_parse_signature_oversize_rejected(void) {
    uint8_t signature[CX_ECDSA_SHA256_SIG_MAX_ASN1_LENGTH + 1];
    payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, OMIT(WIRE_TAG_SIGNATURE));

    memset(signature, 0x42, sizeof(signature));
    put(&p, WIRE_TAG_SIGNATURE, signature, sizeof(signature));
    assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
}

// =============================================================================
// Mandatory tags, version, signature
// =============================================================================

void test_parse_sign_tx_missing_required_tag_rejected(void) {
    const uint8_t required[] = {
        WIRE_TAG_VERSION,
        WIRE_TAG_CHAIN_ID,
        WIRE_TAG_CONTRACT_ADDR,
        WIRE_TAG_SELECTOR,
        WIRE_TAG_FIELDS_HASH,
        WIRE_TAG_OPERATION_TYPE,
        WIRE_TAG_SIGNATURE,
    };

    for (size_t i = 0; i < ARRAYLEN(required); i++) {
        payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, OMIT(required[i]));

        assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
    }
}

void test_parse_empty_payload_rejected(void) {
    payload_t p = {0};

    assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
    assert_rejected(&p, OPERATION_INFO_CTX_EIP712);
}

// VERSION is the only tag: separate code path from "missing VERSION", the other required tags
// are checked after it
void test_parse_version_alone_rejected(void) {
    payload_t p = {0};

    put(&p, WIRE_TAG_VERSION, k_version, sizeof(k_version));
    assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
    assert_rejected(&p, OPERATION_INFO_CTX_EIP712);
}

void test_parse_unsupported_version_rejected(void) {
    static const uint8_t version[] = {0x99};
    const e_operation_info_context contexts[] = {OPERATION_INFO_CTX_SIGN_TX,
                                                 OPERATION_INFO_CTX_EIP712};

    for (size_t i = 0; i < ARRAYLEN(contexts); i++) {
        payload_t p = valid(contexts[i], OMIT(WIRE_TAG_VERSION));

        put(&p, WIRE_TAG_VERSION, version, sizeof(version));
        assert_rejected(&p, contexts[i]);
    }
}

void test_parse_signature_check_failure_rejected(void) {
    payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, 0);

    s_sig_check_ret = false;
    assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
}

void test_parse_finalize_hash_failure_rejected(void) {
    payload_t p = valid(OPERATION_INFO_CTX_SIGN_TX, 0);

    s_finalize_hash_ret = false;
    assert_rejected(&p, OPERATION_INFO_CTX_SIGN_TX);
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
    RUN_TEST(test_getters_null_operation_info_returns_null);
    RUN_TEST(test_getters_empty_string_returns_null);
    RUN_TEST(test_getters_return_populated_fields);
    RUN_TEST(test_delete_operation_info_frees_node);
    RUN_TEST(test_parse_sign_tx_populates_required_fields);
    RUN_TEST(test_parse_optional_metadata_tags_populated);
    RUN_TEST(test_parse_selector_wrong_size_rejected);
    RUN_TEST(test_parse_fields_hash_of_another_size_rejected);
    RUN_TEST(test_parse_deploy_date_oversized_rejected);
    RUN_TEST(test_parse_signature_oversize_rejected);
    RUN_TEST(test_parse_sign_tx_missing_required_tag_rejected);
    RUN_TEST(test_parse_empty_payload_rejected);
    RUN_TEST(test_parse_version_alone_rejected);
    RUN_TEST(test_parse_unsupported_version_rejected);
    RUN_TEST(test_parse_signature_check_failure_rejected);
    RUN_TEST(test_parse_finalize_hash_failure_rejected);
    return UNITY_END();
}
