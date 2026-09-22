/**
 * @file test_cmd_get_gating.c
 * @brief Unit tests for the backend-signed gated-signing descriptor at
 *        src/features/provide_gating/cmd_get_gating.c.
 *
 * The gated-signing descriptor lets the backend nudge the user toward a
 * safer alternative when the current sign request is blind. The
 * descriptor binds:
 *   - the chain_id (for SignTx flows),
 *   - the contract address (must match the active tx),
 *   - the function selector or EIP-712 schema hash,
 *   - the intro message + tiny URL that show up on the prelude screen.
 *
 * After verify_signature succeeds against
 * CERTIFICATE_PUBLIC_KEY_USAGE_GATED_SIGNING, the device must still
 * cross-check that the descriptor binds to the *same* transaction the
 * user is reviewing (type / chain_id / address / selector). A bug in
 * either gate lets a malicious backend hide the gating prelude (or
 * worse, render it for the wrong tx).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "shared_context.h"
#include "cmd_get_gating.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "eip712_v1_context.h"
#include "nbgl_use_case.h"
#include "nbgl_types.h"
#include "wraps.h"
#include "Mocknetwork.h"

// =============================================================================
// Network mock state
// =============================================================================

static uint64_t s_tx_chain_id = 1;
static uint64_t get_tx_chain_id_stub(int cmock_num_calls) {
    (void) cmock_num_calls;
    return s_tx_chain_id;
}

// `warning` is referenced by set_gating_ui_screen; the real symbol
// lives in libNbgl which we don't link here, so provide local storage.

// =============================================================================
// Globals required by linked translation units
// =============================================================================

s_eip712_v1_context g_eip712_storage;
s_eip712_v1_context *eip712_v1_context = NULL;

// LARGE_LEDGER_ICON maps to C_Ledger_64px or C_Ledger_14px depending on
// SCREEN_SIZE_WALLET. Define both so the linker is satisfied either way.
const nbgl_icon_details_t C_Ledger_64px;
const nbgl_icon_details_t C_Ledger_14px;

// =============================================================================
// Controllable stubs
// =============================================================================

static bool s_sig_check_ret = true;
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
    return s_sig_check_ret;
}

static bool s_finalize_hash_ret = true;
bool finalize_hash(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len) {
    (void) hash_ctx;
    (void) out;
    (void) out_len;
    return s_finalize_hash_ret;
}

static bool g_compute_schema_hash_ret = true;
static uint8_t g_schema_hash_buf[CX_SHA224_SIZE];
bool compute_schema_hash(uint8_t hash[CX_SHA224_SIZE]) {
    memcpy(hash, g_schema_hash_buf, CX_SHA224_SIZE);
    return g_compute_schema_hash_ret;
}

static uint8_t g_domain_contract_addr[ADDRESS_LENGTH];
bool td_get_domain_contract_addr(uint8_t addr[ADDRESS_LENGTH]) {
    memcpy(addr, g_domain_contract_addr, ADDRESS_LENGTH);
    return true;
}

bool td_get_domain_chain_id(uint64_t *chain_id) {
    (void) chain_id;
    return false;
}

// Proxy lookup wraps — return NULL by default (no proxy).
static const uint8_t *g_implem_contract_ret = NULL;
static const uint8_t *g_proxy_contract_ret = NULL;
const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *contract,
                                   const uint8_t *selector) {
    (void) chain_id;
    (void) contract;
    (void) selector;
    return g_implem_contract_ret;
}
const uint8_t *get_proxy_contract(const uint64_t *chain_id,
                                  const uint8_t *implem,
                                  const uint8_t *selector) {
    (void) chain_id;
    (void) implem;
    (void) selector;
    return g_proxy_contract_ret;
}

// nvm_write wrap — capture the value the source pushes into the
// gating_counter slot so tests can verify the counter cadence.
static uint8_t g_captured_counter = 0;
static int g_nvm_write_calls = 0;
void nvm_write(void *dst, void *src, unsigned int len) {
    (void) dst;
    if (len == 1 && src != NULL) {
        g_captured_counter = *(uint8_t *) src;
    }
    g_nvm_write_calls++;
    // Reflect the write back into N_storage_real so subsequent calls
    // see the bumped counter.
    g_n_storage_writable.gating_counter = g_captured_counter;
}

// =============================================================================
// TLV builder for GATING descriptor
// =============================================================================
//
// Tags ≥ 0x80 need DER long-form (0x81 <tag>).
//   0x01 STRUCTURE_TYPE      = 0x0D
//   0x02 STRUCTURE_VERSION   = 0x01
//   0x22 ADDRESS             = 20 bytes
//   0x23 CHAIN_ID            = 1+ bytes
//   0x40 HASH_SELECTOR       = 4 bytes (Tx) or 28 bytes (EIP-712)
//   0x82 INTRO_MSG           = string (long-form)
//   0x83 TINY_URL            = string (long-form)
//   0x84 TX_TYPE             = 1 byte (long-form): 0=Tx, 1=TypedData
//   0x15 DER_SIGNATURE       = N bytes

#define ECDSA_SIG_MIN 67
#define ECDSA_SIG_MAX 72

typedef struct {
    uint8_t struct_type;
    uint8_t struct_version;
    uint8_t chain_id;
    bool include_chain_id;
    bool include_hash_selector;
    bool include_address;
    bool include_intro_msg;
    bool include_tiny_url;
    bool include_tx_type;
    bool include_signature;
    bool address_zero;
    uint8_t hash_selector_size;
    uint8_t tx_type;
    uint8_t address_byte;
    uint8_t selector_byte;
    uint8_t sig_len;
} s_opts;

static s_opts default_tx_opts(void) {
    s_opts o = {.struct_type = 0x0D,
                .struct_version = 0x01,
                .chain_id = 1,
                .include_chain_id = true,
                .include_hash_selector = true,
                .include_address = true,
                .include_intro_msg = true,
                .include_tiny_url = true,
                .include_tx_type = true,
                .include_signature = true,
                .hash_selector_size = 4,  // SignTx = selector
                .tx_type = 0,             // TX_TYPE_TRANSACTION (0 in wire, +1 in struct)
                .address_byte = 0xAA,
                .selector_byte = 0xCC,
                .sig_len = ECDSA_SIG_MIN};
    return o;
}

static s_opts default_eip712_opts(void) {
    s_opts o = default_tx_opts();
    o.include_chain_id = false;
    o.hash_selector_size = 28;  // CX_SHA224_SIZE
    o.tx_type = 1;              // TX_TYPE_TYPED_DATA wire value
    return o;
}

static void w(uint8_t *out, size_t *off, uint8_t b) {
    out[(*off)++] = b;
}

static size_t build_tlv(uint8_t *out, size_t out_size, s_opts opts) {
    size_t off = 0;
    // STRUCTURE_TYPE
    w(out, &off, 0x01);
    w(out, &off, 0x01);
    w(out, &off, opts.struct_type);
    // STRUCTURE_VERSION
    w(out, &off, 0x02);
    w(out, &off, 0x01);
    w(out, &off, opts.struct_version);
    // ADDRESS
    if (opts.include_address) {
        w(out, &off, 0x22);
        w(out, &off, ADDRESS_LENGTH);
        memset(out + off, opts.address_zero ? 0 : opts.address_byte, ADDRESS_LENGTH);
        off += ADDRESS_LENGTH;
    }
    // CHAIN_ID
    if (opts.include_chain_id) {
        w(out, &off, 0x23);
        w(out, &off, 0x01);
        w(out, &off, opts.chain_id);
    }
    // HASH_SELECTOR
    if (opts.include_hash_selector) {
        w(out, &off, 0x40);
        w(out, &off, opts.hash_selector_size);
        memset(out + off, opts.selector_byte, opts.hash_selector_size);
        off += opts.hash_selector_size;
    }
    // INTRO_MSG (tag 0x82 needs long-form)
    if (opts.include_intro_msg) {
        w(out, &off, 0x81);
        w(out, &off, 0x82);
        w(out, &off, 0x05);
        memcpy(out + off, "hello", 5);
        off += 5;
    }
    // TINY_URL (tag 0x83 needs long-form)
    if (opts.include_tiny_url) {
        w(out, &off, 0x81);
        w(out, &off, 0x83);
        w(out, &off, 0x04);
        memcpy(out + off, "http", 4);
        off += 4;
    }
    // TX_TYPE (tag 0x84 needs long-form)
    if (opts.include_tx_type) {
        w(out, &off, 0x81);
        w(out, &off, 0x84);
        w(out, &off, 0x01);
        w(out, &off, opts.tx_type);
    }
    // SIGNATURE
    if (opts.include_signature) {
        w(out, &off, 0x15);
        w(out, &off, opts.sig_len);
        // DER-shape stub: 0x30 LEN 0x02 .. 0x02 ..  (parser only checks size range)
        out[off] = 0x30;
        out[off + 1] = opts.sig_len - 2;
        for (size_t i = 2; i < opts.sig_len; i++) {
            out[off + i] = 0x42;
        }
        off += opts.sig_len;
    }
    TEST_ASSERT_TRUE(off <= out_size);
    return off;
}

static bool send_descriptor(const uint8_t *tlv, size_t len) {
    // Prepend the BE16 length prefix tlv_apdu expects on the first chunk.
    uint8_t framed[600];
    framed[0] = (uint8_t) (len >> 8);
    framed[1] = (uint8_t) (len & 0xFF);
    TEST_ASSERT_TRUE(len + 2 <= sizeof(framed));
    memcpy(framed + 2, tlv, len);
    uint16_t sw = handle_gating(/*p1=*/P1_FIRST_CHUNK, /*p2=*/0x00, framed, (uint8_t) (len + 2));
    return sw == SWO_SUCCESS;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    clear_gating();
    memset(&g_n_storage_writable, 0, sizeof(g_n_storage_writable));
    memset(&g_eip712_storage, 0, sizeof(g_eip712_storage));
    eip712_v1_context = NULL;
    s_tx_chain_id = 1;
    s_sig_check_ret = true;
    s_finalize_hash_ret = true;
    g_compute_schema_hash_ret = true;
    g_implem_contract_ret = NULL;
    g_proxy_contract_ret = NULL;
    memset(g_schema_hash_buf, 0, sizeof(g_schema_hash_buf));
    memset(g_domain_contract_addr, 0, sizeof(g_domain_contract_addr));
    g_captured_counter = 0;
    g_nvm_write_calls = 0;
    memset(&txContext, 0, sizeof(txContext));
    memset(&tmpContent, 0, sizeof(tmpContent));
    appState = APP_STATE_SIGNING_TX;
    tlv_from_apdu(false, 0, NULL, NULL);
}

// =============================================================================
// Tests — entry-point dispatcher
// =============================================================================

void test_p2_unknown_rejected(void) {
    uint8_t data[1] = {0};
    uint16_t sw = handle_gating(/*p1=*/0x00, /*p2=*/0xFF, data, 1);
    TEST_ASSERT_EQUAL(sw, SWO_WRONG_P1_P2);
}

// =============================================================================
// TLV happy path + validation gates
// =============================================================================

void test_happy_path_transaction(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(send_descriptor(tlv, len));
}

void test_happy_path_typed_data(void) {
    appState = APP_STATE_SIGNING_EIP712;
    uint8_t tlv[500];
    s_opts opts = default_eip712_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(send_descriptor(tlv, len));
}

void test_invalid_struct_type_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.struct_type = 0xFF;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_invalid_struct_version_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.struct_version = 0x05;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_address_all_zeros_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.address_zero = true;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_hash_selector_too_big_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.hash_selector_size = 32;  // > CX_SHA224_SIZE
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_tx_type_out_of_range_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.tx_type = 0x05;  // beyond TX_TYPE_TYPED_DATA wire value
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_signtx_without_chain_id_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.include_chain_id = false;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_typed_data_without_hash_selector_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_eip712_opts();
    opts.include_hash_selector = false;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_missing_address_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.include_address = false;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_missing_intro_msg_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.include_intro_msg = false;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_missing_tiny_url_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.include_tiny_url = false;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_missing_signature_rejected(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    opts.include_signature = false;
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
}

void test_signature_check_failure_clears_gating(void) {
    s_sig_check_ret = false;
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_FALSE(send_descriptor(tlv, len));
    // After a failed verify_signature, set_gating_warning must report
    // no descriptor is active.
    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_nvm_write_calls, 0);
}

void test_clear_gating_wipes_descriptor(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(send_descriptor(tlv, len));
    clear_gating();
    // No GATING means set_gating_warning short-circuits to true.
    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_nvm_write_calls, 0);
}

// =============================================================================
// set_gating_warning — UI cross-check
// =============================================================================

// Prime a SignTx descriptor and arm the active-tx state so that
// check_tx_gating_params would succeed without further tweaks.
static void prime_tx_descriptor(void) {
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(send_descriptor(tlv, len));
    appState = APP_STATE_SIGNING_TX;
    s_tx_chain_id = 1;
    memset(tmpContent.txContent.destination, 0xAA, ADDRESS_LENGTH);
    memset(txContext.selector_bytes, 0xCC, SELECTOR_SIZE);
}

void test_set_warning_no_descriptor_returns_true(void) {
    // GATING is NULL (reset cleared it) — must be permissive.
    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_nvm_write_calls, 0);
}

void test_set_warning_happy_path_bumps_counter(void) {
    prime_tx_descriptor();
    TEST_ASSERT_TRUE(set_gating_warning());
    // 0 + 1 = 1, and 1 % 10 == 1 → screen shown, prelude wired up.
    TEST_ASSERT_EQUAL(g_captured_counter, 1);
    TEST_ASSERT_NOT_NULL(warning.prelude);
}

void test_clear_gating_drops_the_prelude(void) {
    // The prelude strings point into the GATING block and NBGL re-reads them at
    // every page build, so freeing the block must take warning.prelude with it.
    prime_tx_descriptor();
    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_NOT_NULL(warning.prelude);
    clear_gating();
    TEST_ASSERT_NULL(warning.prelude);
}

void test_second_descriptor_drops_the_previous_prelude(void) {
    // A new payload frees the block the live prelude points into before parsing
    // into a fresh one; the old pointers must not survive that free.
    uint8_t tlv[500];
    s_opts opts = default_tx_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);

    prime_tx_descriptor();
    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_NOT_NULL(warning.prelude);

    TEST_ASSERT_TRUE(send_descriptor(tlv, len));
    TEST_ASSERT_NULL(warning.prelude);
}

void test_set_warning_type_mismatch_returns_false(void) {
    prime_tx_descriptor();
    appState = APP_STATE_SIGNING_EIP712;  // descriptor.type = TX
    TEST_ASSERT_FALSE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_nvm_write_calls, 0);
}

void test_set_warning_chain_id_mismatch_returns_false(void) {
    prime_tx_descriptor();
    s_tx_chain_id = 137;  // descriptor signed for chain 1
    TEST_ASSERT_FALSE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_nvm_write_calls, 0);
}

void test_set_warning_address_mismatch_returns_false(void) {
    prime_tx_descriptor();
    memset(tmpContent.txContent.destination, 0x77, ADDRESS_LENGTH);
    TEST_ASSERT_FALSE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_nvm_write_calls, 0);
}

void test_set_warning_selector_mismatch_returns_false(void) {
    prime_tx_descriptor();
    memset(txContext.selector_bytes, 0x99, SELECTOR_SIZE);
    TEST_ASSERT_FALSE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_nvm_write_calls, 0);
}

void test_set_warning_counter_skips_non_modulo(void) {
    prime_tx_descriptor();
    // Pretend we already showed the prelude once (counter=1) — next call
    // bumps to 2, which is not 1 mod 10, so warning.prelude must NOT be
    // rewritten but the call must still succeed.
    g_n_storage_writable.gating_counter = 1;
    warning.prelude = NULL;
    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_captured_counter, 2);
    TEST_ASSERT_NULL(warning.prelude);
}

void test_set_warning_counter_wrap_reanchors_to_one(void) {
    prime_tx_descriptor();
    // Counter at 0xFF wraps to 0; the source must re-anchor to 1 so
    // the cadence stays aligned across the wrap.
    g_n_storage_writable.gating_counter = 0xFF;
    warning.prelude = NULL;
    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_captured_counter, 1);
    TEST_ASSERT_NOT_NULL(warning.prelude);
}

void test_set_warning_proxy_implementation_match(void) {
    prime_tx_descriptor();
    // Pretend the active tx hits a proxy that resolves to the descriptor
    // address. The proxy lookup returns a contract address that matches
    // the tx destination — gate must accept.
    static const uint8_t implem[ADDRESS_LENGTH] = {0xAA};
    static const uint8_t proxy[ADDRESS_LENGTH] = {0xAA};
    g_implem_contract_ret = implem;
    g_proxy_contract_ret = proxy;
    // Implementation must equal GATING->address (0xAA pattern). Make the
    // returned proxy address equal tmpContent.txContent.destination so the
    // final memcmp passes.
    static uint8_t proxy_full[ADDRESS_LENGTH];
    memset(proxy_full, 0xAA, sizeof(proxy_full));
    static uint8_t implem_full[ADDRESS_LENGTH];
    memset(implem_full, 0xAA, sizeof(implem_full));
    g_implem_contract_ret = implem_full;
    g_proxy_contract_ret = proxy_full;
    memset(tmpContent.txContent.destination, 0xAA, ADDRESS_LENGTH);
    TEST_ASSERT_TRUE(set_gating_warning());
}

void test_set_warning_proxy_implementation_mismatch(void) {
    prime_tx_descriptor();
    // Proxy lookup says the implementation is some unrelated address ⇒
    // gate must reject regardless of the descriptor signature.
    static uint8_t implem_full[ADDRESS_LENGTH];
    memset(implem_full, 0x55, sizeof(implem_full));
    g_implem_contract_ret = implem_full;
    TEST_ASSERT_FALSE(set_gating_warning());
}

void test_set_warning_typed_data_happy_path(void) {
    appState = APP_STATE_SIGNING_EIP712;
    eip712_v1_context = &g_eip712_storage;
    memset(g_domain_contract_addr, 0xAA, ADDRESS_LENGTH);
    memset(g_schema_hash_buf, 0xCC, sizeof(g_schema_hash_buf));

    uint8_t tlv[500];
    s_opts opts = default_eip712_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(send_descriptor(tlv, len));

    TEST_ASSERT_TRUE(set_gating_warning());
    TEST_ASSERT_EQUAL(g_captured_counter, 1);
}

void test_set_warning_typed_data_schema_hash_mismatch(void) {
    appState = APP_STATE_SIGNING_EIP712;
    eip712_v1_context = &g_eip712_storage;
    memset(g_domain_contract_addr, 0xAA, ADDRESS_LENGTH);
    memset(g_schema_hash_buf, 0x33, sizeof(g_schema_hash_buf));  // mismatch

    uint8_t tlv[500];
    s_opts opts = default_eip712_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(send_descriptor(tlv, len));

    TEST_ASSERT_FALSE(set_gating_warning());
}

void test_set_warning_typed_data_compute_schema_failure(void) {
    appState = APP_STATE_SIGNING_EIP712;
    eip712_v1_context = &g_eip712_storage;
    memset(g_domain_contract_addr, 0xAA, ADDRESS_LENGTH);
    g_compute_schema_hash_ret = false;

    uint8_t tlv[500];
    s_opts opts = default_eip712_opts();
    size_t len = build_tlv(tlv, sizeof(tlv), opts);
    TEST_ASSERT_TRUE(send_descriptor(tlv, len));

    TEST_ASSERT_FALSE(set_gating_warning());
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocknetwork_Init();
    get_tx_chain_id_StubWithCallback(get_tx_chain_id_stub);
    reset();
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_p2_unknown_rejected);
    RUN_TEST(test_happy_path_transaction);
    RUN_TEST(test_happy_path_typed_data);
    RUN_TEST(test_invalid_struct_type_rejected);
    RUN_TEST(test_invalid_struct_version_rejected);
    RUN_TEST(test_address_all_zeros_rejected);
    RUN_TEST(test_hash_selector_too_big_rejected);
    RUN_TEST(test_tx_type_out_of_range_rejected);
    RUN_TEST(test_signtx_without_chain_id_rejected);
    RUN_TEST(test_typed_data_without_hash_selector_rejected);
    RUN_TEST(test_missing_address_rejected);
    RUN_TEST(test_missing_intro_msg_rejected);
    RUN_TEST(test_missing_tiny_url_rejected);
    RUN_TEST(test_missing_signature_rejected);
    RUN_TEST(test_signature_check_failure_clears_gating);
    RUN_TEST(test_clear_gating_wipes_descriptor);
    RUN_TEST(test_set_warning_no_descriptor_returns_true);
    RUN_TEST(test_set_warning_happy_path_bumps_counter);
    RUN_TEST(test_clear_gating_drops_the_prelude);
    RUN_TEST(test_second_descriptor_drops_the_previous_prelude);
    RUN_TEST(test_set_warning_type_mismatch_returns_false);
    RUN_TEST(test_set_warning_chain_id_mismatch_returns_false);
    RUN_TEST(test_set_warning_address_mismatch_returns_false);
    RUN_TEST(test_set_warning_selector_mismatch_returns_false);
    RUN_TEST(test_set_warning_counter_skips_non_modulo);
    RUN_TEST(test_set_warning_counter_wrap_reanchors_to_one);
    RUN_TEST(test_set_warning_proxy_implementation_match);
    RUN_TEST(test_set_warning_proxy_implementation_mismatch);
    RUN_TEST(test_set_warning_typed_data_happy_path);
    RUN_TEST(test_set_warning_typed_data_schema_hash_mismatch);
    RUN_TEST(test_set_warning_typed_data_compute_schema_failure);
    return UNITY_END();
}
