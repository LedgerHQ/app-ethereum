/**
 * @file test_param_calldata.c
 * @brief Unit tests for CALLDATA parameter formatting, focusing on
 *        §3.1.8 iteration broadcast (contract_addr of size 1 repeated
 *        across all calldata iterations).
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gtp_param_calldata.h"
#include "gtp_parsed_value.h"
#include "gtp_value.h"
#include "tx_ctx.h"
#include "calldata.h"
#include "shared_context.h"
#include "eth_ustream.h"

// Required globals

// ===========================================================================
// value_get / value_cleanup mock
// ===========================================================================

static int g_vg_call = 0;
static s_parsed_value_collection g_vg[2];

static bool g_tx_ctx_init_ret = true;

bool value_get(const s_value *value, s_parsed_value_collection *collection) {
    (void) value;
    *collection = g_vg[g_vg_call++];
    return true;
}

// ===========================================================================
// handle_value_struct stub — TLV parse handlers reference it but are never
// called by format_param_calldata; only needed for linkage.
// ===========================================================================

bool handle_value_struct(const buffer_t *buf, s_value_context *context) {
    (void) buf;
    (void) context;
    return true;
}

// ===========================================================================
// Calldata stubs — only needed for linkage; never called with empty calldata
// ===========================================================================

// calldata_init / calldata_append controllable via globals — the
// nested-calldata tests below need both success and failure variants.
// We malloc a real buffer because the production code calls
// APP_MEM_FREE on a calldata-append failure, which in test mode lands
// in free() — a static sentinel would crash with "invalid size".
static bool g_calldata_init_ok = false;  // default: NULL (legacy tests rely on this)
static bool g_calldata_append_ok = true;
static int g_calldata_delete_calls = 0;

s_calldata *calldata_init(size_t size, const uint8_t *selector) {
    (void) size;
    (void) selector;
    return g_calldata_init_ok ? (s_calldata *) calloc(1, sizeof(s_calldata)) : NULL;
}

bool calldata_append(s_calldata *calldata, const uint8_t *data, size_t length) {
    (void) calldata;
    (void) data;
    (void) length;
    return g_calldata_append_ok;
}

void calldata_delete(s_calldata *calldata) {
    (void) calldata;
    g_calldata_delete_calls++;
}

// ===========================================================================
// tx_ctx_init mock
// ===========================================================================

bool tx_ctx_init(s_calldata *calldata,
                 const uint8_t *from,
                 const uint8_t *to,
                 const uint8_t *amount,
                 const uint64_t *chain_id) {
    (void) calldata;
    (void) from;
    (void) to;
    (void) amount;
    (void) chain_id;
    return (bool) g_tx_ctx_init_ret;
}

// ===========================================================================
// Test data
// ===========================================================================

// Fake contract address (20 bytes) — single entry to be broadcast
static uint8_t g_contract_addr[ADDRESS_LENGTH] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD,
    0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
};

// ===========================================================================
// Tests
// ===========================================================================

/**
 * Broadcast: calldata collection has 2 elements (both empty, length=0),
 * contract_addr collection has 1 element — broadcast to both iterations.
 * Expected: format_param_calldata returns true and tx_ctx_init is called twice.
 */
void test_calldata_broadcast_ok(void) {
    // Primary collection: two empty calldatas (length=0 skips calldata_init)
    g_vg[0].size = 2;
    g_vg[0].value[0] = (s_parsed_value) {.ptr = NULL, .size = 0, .offset = 0, .length = 0};
    g_vg[0].value[1] = (s_parsed_value) {.ptr = NULL, .size = 0, .offset = 0, .length = 0};

    // Secondary (contract_addr) collection: one address — broadcast
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {
        .ptr = g_contract_addr,
        .size = ADDRESS_LENGTH,
        .offset = 0,
        .length = ADDRESS_LENGTH,
    };

    g_vg_call = 0;
    memset(&txContext, 0, sizeof(txContext));

    // tx_ctx_init called once per calldata iteration (2 iterations)
    g_tx_ctx_init_ret = true;
    g_tx_ctx_init_ret = true;

    s_param_calldata param;
    memset(&param, 0, sizeof(param));
    param.version = 1;

    TEST_ASSERT_TRUE(format_param_calldata(&param, "Destination"));

    // batch_nb_tx incremented by calldatas.size (2) since size > 1
    TEST_ASSERT_EQUAL(txContext.batch_nb_tx, 2);
    TEST_ASSERT_EQUAL(txContext.current_batch_size, 2);
}

/**
 * Mismatch rejection: calldata collection has 2 elements,
 * contract_addr collection has 3 — neither equals 1 nor matches calldatas.size.
 * check_param must return false; tx_ctx_init must NOT be called.
 */
void test_calldata_size_mismatch_rejected(void) {
    // Primary collection: 2 calldatas
    g_vg[0].size = 2;

    // Secondary (contract_addr) collection: 3 — mismatched
    g_vg[1].size = 3;

    g_vg_call = 0;
    memset(&txContext, 0, sizeof(txContext));

    s_param_calldata param;
    memset(&param, 0, sizeof(param));
    param.version = 1;

    // format_param_calldata must return false; tx_ctx_init is NOT called
    TEST_ASSERT_FALSE(format_param_calldata(&param, "Destination"));
}

// ===========================================================================
// Test runner
// ===========================================================================

// ===========================================================================
// TLV tag-handler tests — drive handle_param_calldata_struct with
// hand-crafted TLV buffers. The wrapped handle_value_struct ignores its
// inner buffer, so VALUE / CALLEE / CHAIN_ID / SELECTOR / AMOUNT /
// SPENDER tags all reduce to a "did the dispatch route here?" check via
// the has_X side-effects.
//
// All tags are < 0x80 so short-form encoding is used.
// ===========================================================================

void test_handle_calldata_struct_all_tags_ok(void) {
    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x01,
        0x00,  // VALUE
        0x02,
        0x00,  // CALLEE
        0x03,
        0x00,  // CHAIN_ID
        0x04,
        0x00,  // SELECTOR
        0x05,
        0x00,  // AMOUNT
        0x06,
        0x00,  // SPENDER
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_calldata param;
    memset(&param, 0, sizeof(param));
    s_param_calldata_context ctx = {.param = &param};
    TEST_ASSERT_TRUE(handle_param_calldata_struct(&buf, &ctx));
    TEST_ASSERT_EQUAL(param.version, 1);
    TEST_ASSERT_TRUE(param.has_chain_id);
    TEST_ASSERT_TRUE(param.has_selector);
    TEST_ASSERT_TRUE(param.has_amount);
    TEST_ASSERT_TRUE(param.has_spender);
}

void test_handle_calldata_struct_version_only_rejected(void) {
    // VERSION alone is not enough: VALUE and CALLEE are mandatory, an empty
    // or partial PARAM payload must not parse
    uint8_t buf_bytes[] = {
        0x00,
        0x01,
        0x03,  // VERSION = 3
    };
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};

    s_param_calldata param;
    memset(&param, 0, sizeof(param));
    s_param_calldata_context ctx = {.param = &param};
    TEST_ASSERT_FALSE(handle_param_calldata_struct(&buf, &ctx));
    TEST_ASSERT_FALSE(param.has_chain_id);
    TEST_ASSERT_FALSE(param.has_selector);
    TEST_ASSERT_FALSE(param.has_amount);
    TEST_ASSERT_FALSE(param.has_spender);
}

// ===========================================================================
// process_nested_calldata — non-empty calldata payload paths
// ===========================================================================
//
// The base broadcast tests drive process_nested_calldata with empty
// (length=0) calldatas, which skips the entire calldata-init block.
// These tests give the calldata real bytes so the nested-call branch
// of the parser is pinned. This is the multicall / Safe execTx surface:
// a bug here lets a sub-call slip past the per-call validation.

static void reset(void) {
    g_vg_call = 0;
    memset(g_vg, 0, sizeof(g_vg));
    memset(&txContext, 0, sizeof(txContext));
    g_calldata_init_ok = false;
    g_calldata_append_ok = true;
    g_calldata_delete_calls = 0;
}

// Sample 8-byte nested calldata: 4-byte selector + 4 arg bytes.
static uint8_t g_nested_calldata_bytes[8] = {
    0xA9,
    0x05,
    0x9C,
    0xBB,  // selector
    0x01,
    0x02,
    0x03,
    0x04,  // args
};

void test_nested_calldata_with_selector_happy_path(void) {
    g_calldata_init_ok = true;  // calldata_init returns sentinel
    // Primary: 1 non-empty calldata
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {
        .ptr = g_nested_calldata_bytes,
        .size = 8,
        .offset = 0,
        .length = 8,
    };
    // Secondary: contract_addr — 1 entry
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {
        .ptr = g_contract_addr,
        .size = ADDRESS_LENGTH,
        .offset = 0,
        .length = ADDRESS_LENGTH,
    };
    g_tx_ctx_init_ret = true;

    s_param_calldata param = {0};
    param.version = 1;
    param.has_selector = true;
    // Provide a non-empty selector value via the static collec — we feed
    // a third collection by chaining via value_get's g_vg cursor.
    g_vg[2].size = 1;
    g_vg[2].value[0] = (s_parsed_value) {
        .ptr = g_nested_calldata_bytes,
        .size = 4,
        .offset = 0,
        .length = 4,
    };

    TEST_ASSERT_TRUE(format_param_calldata(&param, "Destination"));
}

void test_nested_calldata_no_selector_short_payload_rejected(void) {
    // calldata length = 3 < CALLDATA_SELECTOR_SIZE → process_nested_calldata
    // rejects before reaching calldata_init.
    g_vg[0].size = 1;
    static uint8_t too_short[3] = {0x01, 0x02, 0x03};
    g_vg[0].value[0] = (s_parsed_value) {
        .ptr = too_short,
        .size = 3,
        .offset = 0,
        .length = 3,
    };
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {
        .ptr = g_contract_addr,
        .size = ADDRESS_LENGTH,
        .offset = 0,
        .length = ADDRESS_LENGTH,
    };

    s_param_calldata param = {0};
    param.version = 1;
    param.has_selector = false;  // selector taken from calldata itself
    TEST_ASSERT_FALSE(format_param_calldata(&param, "Destination"));
}

void test_nested_calldata_init_failure_rejected(void) {
    g_calldata_init_ok = false;  // calldata_init returns NULL
    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {
        .ptr = g_nested_calldata_bytes,
        .size = 8,
        .offset = 0,
        .length = 8,
    };
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {
        .ptr = g_contract_addr,
        .size = ADDRESS_LENGTH,
        .offset = 0,
        .length = ADDRESS_LENGTH,
    };

    s_param_calldata param = {0};
    param.version = 1;
    param.has_selector = false;
    TEST_ASSERT_FALSE(format_param_calldata(&param, "Destination"));
}

void test_nested_calldata_append_failure_rejected(void) {
    g_calldata_init_ok = true;
    g_calldata_append_ok = false;  // append fails after init

    g_vg[0].size = 1;
    g_vg[0].value[0] = (s_parsed_value) {
        .ptr = g_nested_calldata_bytes,
        .size = 8,
        .offset = 0,
        .length = 8,
    };
    g_vg[1].size = 1;
    g_vg[1].value[0] = (s_parsed_value) {
        .ptr = g_contract_addr,
        .size = ADDRESS_LENGTH,
        .offset = 0,
        .length = ADDRESS_LENGTH,
    };

    s_param_calldata param = {0};
    param.version = 1;
    param.has_selector = false;
    TEST_ASSERT_FALSE(format_param_calldata(&param, "Destination"));
    // On append failure the production code releases the half-built
    // calldata through calldata_delete
    TEST_ASSERT_EQUAL(g_calldata_delete_calls, 1);
}

void test_nested_calldata_with_all_optional_fields(void) {
    // Exercise has_chain_id + has_amount + has_spender. Each adds one
    // value_get call → secondary collections.
    g_calldata_init_ok = true;

    static uint8_t chain_id_bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0x01};
    static uint8_t amount_bytes[INT256_LENGTH] = {0};
    amount_bytes[31] = 0x05;
    static uint8_t spender_bytes[ADDRESS_LENGTH] = {[0] = 0x55};

    g_vg[0].size = 1;  // calldata
    g_vg[0].value[0] = (s_parsed_value) {
        .ptr = g_nested_calldata_bytes,
        .size = 8,
        .offset = 0,
        .length = 8,
    };
    g_vg[1].size = 1;  // contract_addr
    g_vg[1].value[0] = (s_parsed_value) {
        .ptr = g_contract_addr,
        .size = ADDRESS_LENGTH,
        .offset = 0,
        .length = ADDRESS_LENGTH,
    };
    g_vg[2].size = 1;  // chain_id
    g_vg[2].value[0] = (s_parsed_value) {
        .ptr = chain_id_bytes,
        .size = 8,
        .offset = 0,
        .length = 8,
    };
    g_vg[3].size = 1;  // amount
    g_vg[3].value[0] = (s_parsed_value) {
        .ptr = amount_bytes,
        .size = INT256_LENGTH,
        .offset = 0,
        .length = INT256_LENGTH,
    };
    g_vg[4].size = 1;  // spender
    g_vg[4].value[0] = (s_parsed_value) {
        .ptr = spender_bytes,
        .size = ADDRESS_LENGTH,
        .offset = 0,
        .length = ADDRESS_LENGTH,
    };
    g_tx_ctx_init_ret = true;

    s_param_calldata param = {0};
    param.version = 1;
    param.has_chain_id = true;
    param.has_amount = true;
    param.has_spender = true;
    // has_selector=false so the selector is taken from the calldata.
    TEST_ASSERT_TRUE(format_param_calldata(&param, "Destination"));
}

// ===========================================================================
// Test runner
// ===========================================================================

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_calldata_broadcast_ok);
    RUN_TEST(test_calldata_size_mismatch_rejected);
    RUN_TEST(test_handle_calldata_struct_all_tags_ok);
    RUN_TEST(test_handle_calldata_struct_version_only_rejected);
    RUN_TEST(test_nested_calldata_with_selector_happy_path);
    RUN_TEST(test_nested_calldata_no_selector_short_payload_rejected);
    RUN_TEST(test_nested_calldata_init_failure_rejected);
    RUN_TEST(test_nested_calldata_append_failure_rejected);
    RUN_TEST(test_nested_calldata_with_all_optional_fields);
    return UNITY_END();
}
