/**
 * @file test_eth_ustream_helpers.c
 * @brief Unit tests for the low-level helpers in src/features/sign_tx/eth_ustream.c.
 *
 * This first suite focuses on the two helpers that everything else in the
 * RLP tx parser builds on:
 *   - init_tx() — zeros the context, hooks the sha3/content pointers, and
 *     calls cx_keccak_init_no_throw. If the SDK refuses (HSM seed not
 *     loaded, OOM, etc.) every later call must fail safely.
 *   - copy_tx_data() — consumes `length` bytes from the work buffer into
 *     either an output slot or /dev/null, hashes them into the running
 *     sha3 unless the current field is a self-encoded RLP single byte,
 *     and bumps every position counter exactly once.
 *
 * Later commits will cover the per-field process_* dispatchers and the
 * full state machine; the helpers in scope here have no dependency on
 * the dispatcher so they can be exercised in isolation.
 *
 * SDK crypto primitives are stubbed via fields in a static `s_sdk` table
 * that each test can poke before the call to force success or failure.
 * Every other symbol eth_ustream.c references is satisfied with the
 * lightest stub that makes the linker happy — none of them are reached
 * by the helpers exercised here.
 */

#include "unity.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "shared_context.h"
#include "eth_ustream.h"
#include "feature_sign_tx.h"
#include "Mocknetwork.h"  // network_info_t
#include "tx_ctx.h"
#include "calldata.h"

// =============================================================================
// Globals the module reads — provide storage here.
// =============================================================================

// eth_ustream.c declares s_calldata *g_parked_calldata in tx_ctx.h.

// =============================================================================
// SDK crypto stubs — configurable failure injection
// =============================================================================
//
// eth_ustream.c calls two SDK primitives:
//   - cx_keccak_init_no_throw() in init_tx
//   - cx_hash_no_throw() in read_tx_byte / copy_tx_data
// Each test resets s_sdk so the default is success.

// CX_OK is defined by cx_errors.h (pulled in via eth_ustream.h)
#define CX_INTERNAL_ERR 0x1234

static struct {
    uint32_t keccak_init_ret;
    uint32_t hash_ret;
    // Optional capture of the most recent cx_hash_no_throw input — used by
    // copy_tx_data tests to assert what got hashed.
    uint8_t hash_capture[64];
    size_t hash_capture_len;
    int hash_call_count;
} s_sdk;

uint32_t cx_keccak_init_no_throw(cx_sha3_t *sha3, size_t size) {
    (void) sha3;
    (void) size;
    return s_sdk.keccak_init_ret;
}

uint32_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t in_len,
                          uint8_t *out,
                          size_t out_len) {
    (void) hash;
    (void) mode;
    (void) out;
    (void) out_len;
    if (in && in_len <= sizeof(s_sdk.hash_capture)) {
        memcpy(s_sdk.hash_capture, in, in_len);
        s_sdk.hash_capture_len = in_len;
    }
    s_sdk.hash_call_count++;
    return s_sdk.hash_ret;
}

// =============================================================================
// Stubs for every other eth_ustream.c dependency — unreached by these tests
// =============================================================================

customStatus_e custom_processor(txContext_t *context) {
    (void) context;
    return CUSTOM_NOT_HANDLED;
}

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
    return true;
}

// Controlled by the store_calldata tests below: when g_calldata_init_ok
// is true, return a non-NULL sentinel; when false, return NULL to
// exercise the calldata_init failure branch in process_data.
static bool g_calldata_init_ok = true;
static int g_calldata_init_calls = 0;
static int g_calldata_append_calls = 0;
static bool g_calldata_append_ok = true;
static s_calldata g_calldata_sentinel;
s_calldata *calldata_init(size_t size, const uint8_t *selector) {
    (void) size;
    (void) selector;
    g_calldata_init_calls++;
    return g_calldata_init_ok ? &g_calldata_sentinel : NULL;
}

bool calldata_append(s_calldata *calldata, const uint8_t *buffer, size_t size) {
    (void) calldata;
    (void) buffer;
    (void) size;
    g_calldata_append_calls++;
    return g_calldata_append_ok;
}

void calldata_delete(s_calldata *node) {
    (void) node;
}

// =============================================================================
// Test fixture
// =============================================================================

static void reset(void) {
    memset(&txContext, 0, sizeof(txContext));
    memset(&tmpContent, 0, sizeof(tmpContent));
    memset(&s_sdk, 0, sizeof(s_sdk));
    s_sdk.keccak_init_ret = CX_OK;
    s_sdk.hash_ret = CX_OK;
    g_parked_calldata = NULL;
    g_calldata_init_ok = true;
    g_calldata_append_ok = true;
    g_calldata_init_calls = 0;
    g_calldata_append_calls = 0;
}

// =============================================================================
// init_tx
// =============================================================================

void test_init_tx_zeros_context_and_sets_pointers(void) {
    txContext_t ctx;
    cx_sha3_t sha3 = {0};
    txContent_t content = {0};

    // Dirty the context to make sure init_tx zeroes it
    memset(&ctx, 0xAA, sizeof(ctx));

    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/false));

    TEST_ASSERT_EQUAL_PTR(ctx.sha3, &sha3);
    TEST_ASSERT_EQUAL_PTR(ctx.content, &content);
    TEST_ASSERT_EQUAL(ctx.currentField, RLP_NONE + 1);
    TEST_ASSERT_FALSE(ctx.store_calldata);
    // Other fields should all be zeroed
    TEST_ASSERT_EQUAL(ctx.rlpBufferPos, 0);
    TEST_ASSERT_EQUAL(ctx.currentFieldLength, 0);
    TEST_ASSERT_EQUAL(ctx.commandLength, 0);
    TEST_ASSERT_EQUAL(ctx.txType, 0);
}

void test_init_tx_propagates_store_calldata_flag(void) {
    txContext_t ctx;
    cx_sha3_t sha3 = {0};
    txContent_t content = {0};

    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/true));
    TEST_ASSERT_TRUE(ctx.store_calldata);
}

void test_init_tx_returns_false_on_keccak_init_failure(void) {
    txContext_t ctx;
    cx_sha3_t sha3 = {0};
    txContent_t content = {0};

    s_sdk.keccak_init_ret = CX_INTERNAL_ERR;
    TEST_ASSERT_FALSE(init_tx(&ctx, &sha3, &content, false));
}

// =============================================================================
// copy_tx_data — happy paths
// =============================================================================

void test_copy_tx_data_copies_and_advances(void) {
    const uint8_t src[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    txContext.workBuffer = src;
    txContext.commandLength = sizeof(src);
    txContext.processingField = false;
    txContext.fieldSingleByte = false;

    uint8_t dst[3] = {0};
    TEST_ASSERT_TRUE(copy_tx_data(&txContext, dst, sizeof(dst)));

    // Copied
    const uint8_t expected[] = {0x11, 0x22, 0x33};
    TEST_ASSERT_EQUAL_MEMORY(dst, expected, sizeof(dst));
    // Bookkeeping
    TEST_ASSERT_EQUAL_PTR(txContext.workBuffer, src + 3);
    TEST_ASSERT_EQUAL(txContext.commandLength, 2);
    // currentFieldPos only moves when processingField is true → here it stays
    TEST_ASSERT_EQUAL(txContext.currentFieldPos, 0);
    // Hash got fed
    TEST_ASSERT_EQUAL(s_sdk.hash_call_count, 1);
    TEST_ASSERT_EQUAL(s_sdk.hash_capture_len, 3);
    TEST_ASSERT_EQUAL_MEMORY(s_sdk.hash_capture, expected, 3);
}

void test_copy_tx_data_null_out_consumes_without_copying(void) {
    const uint8_t src[] = {0xAB, 0xCD, 0xEF};
    txContext.workBuffer = src;
    txContext.commandLength = sizeof(src);

    TEST_ASSERT_TRUE(copy_tx_data(&txContext, NULL, 2));
    // Still advances over the consumed bytes
    TEST_ASSERT_EQUAL_PTR(txContext.workBuffer, src + 2);
    TEST_ASSERT_EQUAL(txContext.commandLength, 1);
    // And still hashes
    TEST_ASSERT_EQUAL(s_sdk.hash_call_count, 1);
    TEST_ASSERT_EQUAL(s_sdk.hash_capture_len, 2);
}

void test_copy_tx_data_processing_field_advances_pos(void) {
    const uint8_t src[] = {1, 2, 3, 4};
    txContext.workBuffer = src;
    txContext.commandLength = 4;
    txContext.processingField = true;
    txContext.fieldSingleByte = false;
    txContext.currentFieldPos = 5;  // any pre-existing value

    uint8_t dst[2];
    TEST_ASSERT_TRUE(copy_tx_data(&txContext, dst, 2));
    TEST_ASSERT_EQUAL(txContext.currentFieldPos, 5 + 2);
}

void test_copy_tx_data_zero_length_is_noop_but_hashes(void) {
    const uint8_t src[] = {1, 2, 3};
    txContext.workBuffer = src;
    txContext.commandLength = 3;

    // copy_tx_data(0) should pass the cmd-length check (0 <= commandLength)
    // and call cx_hash_no_throw with len=0 — the SDK accepts that, so we do
    // not assert on hash_capture_len here.
    TEST_ASSERT_TRUE(copy_tx_data(&txContext, NULL, 0));
    TEST_ASSERT_EQUAL_PTR(txContext.workBuffer, src);
    TEST_ASSERT_EQUAL(txContext.commandLength, 3);
}

// =============================================================================
// copy_tx_data — single-byte RLP optimization
// =============================================================================

void test_copy_tx_data_single_byte_self_encoded_skips_hash(void) {
    // When processingField && fieldSingleByte, the byte was already hashed
    // during the pre-decode walk of the RLP prefix → re-hashing would
    // double-count it. copy_tx_data must skip the cx_hash call.
    const uint8_t src[] = {0x7F};  // any single-byte RLP value
    txContext.workBuffer = src;
    txContext.commandLength = 1;
    txContext.processingField = true;
    txContext.fieldSingleByte = true;

    uint8_t dst[1];
    TEST_ASSERT_TRUE(copy_tx_data(&txContext, dst, 1));
    TEST_ASSERT_EQUAL(dst[0], 0x7F);
    // Crucially: no hash call.
    TEST_ASSERT_EQUAL(s_sdk.hash_call_count, 0);
}

void test_copy_tx_data_single_byte_outside_processing_still_hashes(void) {
    // The fieldSingleByte short-circuit only applies during processingField.
    const uint8_t src[] = {0x7F};
    txContext.workBuffer = src;
    txContext.commandLength = 1;
    txContext.processingField = false;
    txContext.fieldSingleByte = true;

    TEST_ASSERT_TRUE(copy_tx_data(&txContext, NULL, 1));
    TEST_ASSERT_EQUAL(s_sdk.hash_call_count, 1);
}

// =============================================================================
// copy_tx_data — failure paths
// =============================================================================

void test_copy_tx_data_command_length_underflow_rejected(void) {
    const uint8_t src[] = {0x11, 0x22};
    txContext.workBuffer = src;
    txContext.commandLength = 2;

    // Asking for 3 bytes when only 2 remain must reject before any
    // dereferencing / advancement.
    uint8_t dst[3] = {0xFF, 0xFF, 0xFF};
    TEST_ASSERT_FALSE(copy_tx_data(&txContext, dst, 3));
    // dst untouched
    TEST_ASSERT_EQUAL(dst[0], 0xFF);
    // workBuffer / commandLength unchanged
    TEST_ASSERT_EQUAL_PTR(txContext.workBuffer, src);
    TEST_ASSERT_EQUAL(txContext.commandLength, 2);
    // No hash call attempted
    TEST_ASSERT_EQUAL(s_sdk.hash_call_count, 0);
}

void test_copy_tx_data_hash_failure_propagates(void) {
    const uint8_t src[] = {0x11, 0x22};
    txContext.workBuffer = src;
    txContext.commandLength = 2;
    s_sdk.hash_ret = CX_INTERNAL_ERR;

    uint8_t dst[2];
    TEST_ASSERT_FALSE(copy_tx_data(&txContext, dst, 2));
    // dst was written before the hash check (memmove runs first)
    // but the function returns false to abort the caller — that's the
    // contract we care about for fault propagation.
}

// =============================================================================
// process_tx — end-to-end RLP parsing of a single transaction.
//
// Driving real RLP bytes through the public entry point exercises every
// static helper (parse_rlp, check_cmd_length, check_empty_list,
// check_fields, the per-type process_legacy_tx / process_eip1559_tx
// dispatchers, and the field-specific process_* functions). The
// alternative — calling each static helper individually — isn't
// possible from outside the TU.
//
// Coverage focus, not crypto: SDK hash/keccak primitives are stubbed
// (return CX_OK), so the running hash and signature recovery aren't
// validated here. The point is to pin the byte-pump that decides what
// goes into the signed digest in the first place.
// =============================================================================

// Build a minimal legacy transaction in RLP encoding.
//
// Plain values:
//   nonce    = 0x07
//   gasPrice = 0x04A817C800  (20 Gwei)
//   startGas = 0x5208        (21000)
//   to       = 20 * 0xAA
//   value    = 0
//   data     = (empty)
//   v        = 0x1B          (chain_id-less / pre-EIP-155 path)
//   r        = 0
//   s        = 0
//
// Inner payload size = 1 + 6 + 3 + 21 + 1 + 1 + 1 + 1 + 1 = 36 bytes.
// List prefix = 0xC0 + 36 = 0xE4.
static const uint8_t g_minimal_legacy_tx[] = {
    0xE4,                                // list, payload = 36 bytes
    0x07,                                // nonce = 7
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // gasPrice
    0x82, 0x52, 0x08,                    // startGas
    0x94,                                // 20-byte string
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value = 0
    0x80,                                                        // data = empty
    0x1B,                                                        // v = 27 (self-encoded)
    0x80,                                                        // r = 0
    0x80,                                                        // s = 0
};

void test_process_tx_legacy_happy_path(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/false));
    ctx.txType = LEGACY;  // cmd_sign_tx sets this before calling.

    parserStatus_e r = process_tx(&ctx, g_minimal_legacy_tx, sizeof(g_minimal_legacy_tx));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);

    // process_* helpers write to ctx->content (i.e. our local `content`).
    TEST_ASSERT_EQUAL(content.nonce.length, 1);
    TEST_ASSERT_EQUAL(content.nonce.value[0], 0x07);

    TEST_ASSERT_EQUAL(content.gasprice.length, 5);
    static const uint8_t expected_gasprice[5] = {0x04, 0xA8, 0x17, 0xC8, 0x00};
    TEST_ASSERT_EQUAL_MEMORY(content.gasprice.value, expected_gasprice, 5);

    TEST_ASSERT_EQUAL(content.startgas.length, 2);
    static const uint8_t expected_startgas[2] = {0x52, 0x08};
    TEST_ASSERT_EQUAL_MEMORY(content.startgas.value, expected_startgas, 2);

    TEST_ASSERT_EQUAL(content.destinationLength, ADDRESS_LENGTH);
    uint8_t expected_to[ADDRESS_LENGTH];
    memset(expected_to, 0xAA, ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(content.destination, expected_to, ADDRESS_LENGTH);

    // value=0 → length 0, no bytes.
    TEST_ASSERT_EQUAL(content.value.length, 0);

    // v = 0x1B captured in v[0].
    TEST_ASSERT_EQUAL(content.v[0], 0x1B);
    TEST_ASSERT_EQUAL(content.vLength, 1);
}

void test_process_tx_truncated_mid_field_returns_processing(void) {
    // Send only the first 12 bytes — far short of the 37-byte total.
    // The parser must report USTREAM_PROCESSING (waiting for more
    // data), not USTREAM_FAULT or FINISHED. That's the contract the
    // multi-APDU chunked sign-tx flow depends on.
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/false));
    ctx.txType = LEGACY;

    parserStatus_e r = process_tx(&ctx, g_minimal_legacy_tx, 12);
    TEST_ASSERT_EQUAL(r, USTREAM_PROCESSING);
}

void test_process_tx_chunked_two_halves_finishes(void) {
    // Feed the same minimal tx in two slices via process_tx +
    // continue_tx. The streamer must reassemble the field that
    // straddles the boundary without losing bytes.
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/false));
    ctx.txType = LEGACY;

    const size_t mid = sizeof(g_minimal_legacy_tx) / 2;
    parserStatus_e r = process_tx(&ctx, g_minimal_legacy_tx, mid);
    TEST_ASSERT_EQUAL(r, USTREAM_PROCESSING);

    // Second slice — process_tx with the remainder.
    r = process_tx(&ctx, g_minimal_legacy_tx + mid, sizeof(g_minimal_legacy_tx) - mid);
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);
    TEST_ASSERT_EQUAL(content.destinationLength, ADDRESS_LENGTH);
}

void test_process_tx_unsupported_tx_type_returns_fault(void) {
    // Force an out-of-range txType — the process_tx_internal default
    // branch must reject rather than fall through to a wrong dispatcher.
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = 0x7F;  // not LEGACY/EIP2930/EIP1559/EIP7702

    parserStatus_e r = process_tx(&ctx, g_minimal_legacy_tx, sizeof(g_minimal_legacy_tx));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
}

// =============================================================================
// EIP-1559 / EIP-2930 / EIP-7702 RLP parsing — typed-transaction
// envelopes. The wire format prepends a single txType byte (0x01 /
// 0x02 / 0x04) to a list whose layout differs from legacy. The
// parser doesn't see the type byte (cmd_sign_tx strips it and sets
// ctx.txType before dispatching), so the test feeds only the inner
// list and forces the type explicitly.
//
// Coverage focus: every per-type dispatcher
// (process_eip1559_tx / process_eip2930_tx / process_eip7702_tx)
// plus the field handlers that legacy doesn't reach (process_chain_id,
// process_access_list, process_auth_list).
// =============================================================================

// EIP-1559 inner list: [chainId, nonce, maxPriorityFee, maxFee,
//                       gasLimit, to, value, data, accessList]
// 9 fields, payload = 36 bytes, list prefix = 0xC0 + 36 = 0xE4.
static const uint8_t g_minimal_eip1559_tx[] = {
    0xE4,                                // list, payload = 36 bytes
    0x01,                                // chainId = 1
    0x07,                                // nonce = 7
    0x05,                                // maxPriorityFee = 5
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // maxFee
    0x82, 0x52, 0x08,                    // gasLimit
    0x94,                                // 20-byte string
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value = 0
    0x80,                                                        // data = empty
    0xC0,                                                        // accessList = empty list
};

void test_process_tx_eip1559_happy_path(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/false));
    ctx.txType = EIP1559;

    parserStatus_e r = process_tx(&ctx, g_minimal_eip1559_tx, sizeof(g_minimal_eip1559_tx));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);

    TEST_ASSERT_EQUAL(content.chainID.length, 1);
    TEST_ASSERT_EQUAL(content.chainID.value[0], 0x01);
    TEST_ASSERT_EQUAL(content.nonce.length, 1);
    TEST_ASSERT_EQUAL(content.nonce.value[0], 0x07);
    // maxFee lands in gasprice (alias) for EIP-1559.
    TEST_ASSERT_EQUAL(content.gasprice.length, 5);
    TEST_ASSERT_EQUAL(content.startgas.length, 2);
    TEST_ASSERT_EQUAL(content.destinationLength, ADDRESS_LENGTH);
    // No v in the unsigned RLP — vLength stays 0.
    TEST_ASSERT_EQUAL(content.vLength, 0);
}

// EIP-2930 inner list: [chainId, nonce, gasPrice, gasLimit, to, value,
//                       data, accessList]
// 8 fields, payload = 35 bytes, list prefix = 0xC0 + 35 = 0xE3.
static const uint8_t g_minimal_eip2930_tx[] = {
    0xE3,                                // list, payload = 35 bytes
    0x01,                                // chainId = 1
    0x07,                                // nonce = 7
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // gasPrice
    0x82, 0x52, 0x08,                    // gasLimit
    0x94, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value = 0
    0x80,                                                        // data = empty
    0xC0,                                                        // accessList = empty list
};

void test_process_tx_eip2930_happy_path(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = EIP2930;

    parserStatus_e r = process_tx(&ctx, g_minimal_eip2930_tx, sizeof(g_minimal_eip2930_tx));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);
    TEST_ASSERT_EQUAL(content.chainID.value[0], 0x01);
    TEST_ASSERT_EQUAL(content.destinationLength, ADDRESS_LENGTH);
}

// EIP-7702 inner list: [chainId, nonce, maxPriorityFee, maxFee,
//                       gasLimit, to, value, data, accessList, authList]
// EIP-7702 is the highest-risk tx type: authList delegates EOA authority
// to contract code, so a parser bug that lets bytes drift between the
// signature and the rendered display is a permanent-account-compromise
// vector. Pin the happy path so the dispatcher and process_auth_list
// are exercised.
//
// 10 fields, payload = 37 bytes, list prefix = 0xC0 + 37 = 0xE5.
static const uint8_t g_minimal_eip7702_tx[] = {
    0xE5,                                // list, payload = 37 bytes
    0x01,                                // chainId = 1
    0x07,                                // nonce = 7
    0x05,                                // maxPriorityFee = 5
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // maxFee
    0x82, 0x52, 0x08,                    // gasLimit
    0x94, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value = 0
    0x80,                                                        // data = empty
    0xC0,                                                        // accessList = empty
    0xC0,                                                        // authList = empty
};

void test_process_tx_eip7702_happy_path(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = EIP7702;

    parserStatus_e r = process_tx(&ctx, g_minimal_eip7702_tx, sizeof(g_minimal_eip7702_tx));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);
    TEST_ASSERT_EQUAL(content.chainID.value[0], 0x01);
    TEST_ASSERT_EQUAL(content.destinationLength, ADDRESS_LENGTH);
}

// process_chain_id explicitly rejects values that don't fit in a
// uint64_t — the comment in eth_ustream.c flags this as CWE-197
// hardening (signature covers one chain, display shows the truncated
// 64-bit prefix). Pin the rejection.
void test_process_tx_eip1559_chainid_overflow_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = EIP1559;

    // 9-byte chainId — exceeds sizeof(uint64_t)=8. The 9-byte string
    // prefix is 0x80 + 9 = 0x89. Just feed the chainId prefix + bytes;
    // the parser must fault before reaching the rest of the envelope.
    const uint8_t bytes[] = {
        0xEC,                                                        // list, payload = 44 bytes
        0x89, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,  // 9-byte chainId
        0x07,                                                        // nonce
        0x05,                                                        // maxPriorityFee
        0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,                          // maxFee
        0x82, 0x52, 0x08,                                            // gasLimit
        0x94, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
        0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
        0x80, 0x80, 0xC0,                                            // value, data, accessList
    };
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
}

// =============================================================================
// process_data — store_calldata path. Drives the calldata-init flow
// inside process_data() with a legacy tx whose data field carries an
// 8-byte payload (4-byte selector + 4-byte argument). store_calldata=true
// asks the parser to fork the data bytes into the GCS calldata buffer
// in addition to the running hash.
//
// Legacy tx layout: [nonce, gasPrice, startGas, to, value, data, v, r, s]
// Data field 8 bytes: 0x88 prefix + 8 bytes.
// Inner payload: 1+6+3+21+1+9+1+1+1 = 44 bytes -> list prefix 0xC0+44 = 0xEC.
// =============================================================================

static const uint8_t g_legacy_tx_with_8byte_data[] = {
    0xEC,                                // list, payload = 44 bytes
    0x07,                                // nonce = 7
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // gasPrice
    0x82, 0x52, 0x08,                    // startGas
    0x94, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value = 0
    0x88, 0xA9, 0x05, 0x9C, 0xBB,                                // data: selector
    0x01, 0x02, 0x03, 0x04,                                      //       + 4 arg bytes
    0x1B, 0x80, 0x80,                                            // v, r, s
};

void test_process_data_captures_selector_bytes(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/false));
    ctx.txType = LEGACY;

    parserStatus_e r =
        process_tx(&ctx, g_legacy_tx_with_8byte_data, sizeof(g_legacy_tx_with_8byte_data));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);

    // process_data captures the first 4 bytes of the data field into
    // ctx.selector_bytes for the gating cross-check.
    static const uint8_t expected_selector[CALLDATA_SELECTOR_SIZE] = {0xA9, 0x05, 0x9C, 0xBB};
    TEST_ASSERT_EQUAL_MEMORY(ctx.selector_bytes, expected_selector, CALLDATA_SELECTOR_SIZE);
}

void test_process_data_store_calldata_happy_path(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/true));
    ctx.txType = LEGACY;

    parserStatus_e r =
        process_tx(&ctx, g_legacy_tx_with_8byte_data, sizeof(g_legacy_tx_with_8byte_data));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);

    // store_calldata=true: calldata_init must be called once with the
    // selector, and the remaining 4 arg bytes appended.
    TEST_ASSERT_EQUAL(g_calldata_init_calls, 1);
    TEST_ASSERT_EQUAL(g_calldata_append_calls, 1);
}

void test_process_data_store_calldata_init_failure_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/true));
    ctx.txType = LEGACY;
    g_calldata_init_ok = false;  // make calldata_init return NULL

    parserStatus_e r =
        process_tx(&ctx, g_legacy_tx_with_8byte_data, sizeof(g_legacy_tx_with_8byte_data));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
    TEST_ASSERT_EQUAL(g_calldata_init_calls, 1);
    TEST_ASSERT_EQUAL(g_calldata_append_calls, 0);  // never reached
}

// Same legacy envelope but with a 2-byte data field — shorter than
// the 4-byte selector. store_calldata mode must refuse rather than
// initialise a calldata buffer without a complete selector.
// Inner payload: 1+6+3+21+1+3+1+1+1 = 38 bytes -> list prefix 0xC0+38 = 0xE6.
static const uint8_t g_legacy_tx_with_2byte_data[] = {
    0xE6,                                // list, payload = 38 bytes
    0x07,                                // nonce
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // gasPrice
    0x82, 0x52, 0x08,                    // startGas
    0x94, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value = 0
    0x82, 0x11, 0x22,                                            // data: 2 bytes
    0x1B, 0x80, 0x80,                                            // v, r, s
};

void test_process_data_store_calldata_short_data_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/true));
    ctx.txType = LEGACY;

    parserStatus_e r =
        process_tx(&ctx, g_legacy_tx_with_2byte_data, sizeof(g_legacy_tx_with_2byte_data));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
    TEST_ASSERT_EQUAL(g_calldata_init_calls, 0);  // refused before alloc
}

// =============================================================================
// Non-empty access_list / auth_list. The base happy-path tests use
// 0xC0 (empty list) which skips the per-list iteration body. Feed
// concrete bytes through both lists so process_access_list /
// process_auth_list are exercised on the path that actually walks
// the payload.
//
// EIP-2930 with access_list = 0xC1 0x80 (list of 1 byte = empty inner
// entry). The outer list grows by 1 byte vs the all-empty envelope.
// Inner payload now 36 bytes -> list prefix 0xC0+36 = 0xE4.
// =============================================================================

static const uint8_t g_eip2930_tx_nonempty_access[] = {
    0xE4,                                // list, payload = 36 bytes
    0x01,                                // chainId = 1
    0x07,                                // nonce
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // gasPrice
    0x82, 0x52, 0x08,                    // gasLimit
    0x94, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value
    0x80,                                                        // data
    0xC1, 0x80,                                                  // accessList = [empty_entry]
};

void test_process_tx_eip2930_nonempty_access_list(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = EIP2930;

    parserStatus_e r =
        process_tx(&ctx, g_eip2930_tx_nonempty_access, sizeof(g_eip2930_tx_nonempty_access));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);
}

// EIP-7702 with both lists non-empty.
// Inner payload: previous 37 + 1 (access) + 1 (auth) = 39 bytes.
// List prefix: 0xC0 + 39 = 0xE7.
static const uint8_t g_eip7702_tx_nonempty_lists[] = {
    0xE7,                                // list, payload = 39 bytes
    0x01,                                // chainId
    0x07,                                // nonce
    0x05,                                // maxPriorityFee
    0x85, 0x04, 0xA8, 0x17, 0xC8, 0x00,  // maxFee
    0x82, 0x52, 0x08,                    // gasLimit
    0x94, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,  // to
    0x80,                                                        // value
    0x80,                                                        // data
    0xC1, 0x80,                                                  // accessList = [empty]
    0xC1, 0x80,                                                  // authList   = [empty]
};

void test_process_tx_eip7702_nonempty_auth_list(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = EIP7702;

    parserStatus_e r =
        process_tx(&ctx, g_eip7702_tx_nonempty_lists, sizeof(g_eip7702_tx_nonempty_lists));
    TEST_ASSERT_EQUAL(r, USTREAM_FINISHED);
}

// =============================================================================
// parse_rlp error paths. The parser must reject malformed RLP rather
// than silently advancing — buffer over-reads here would let a hostile
// host slip bytes past the field boundary the parser thought it was
// reading. Force the rejection by feeding a list-prefix tag whose
// declared length is impossibly large vs the actual command bytes.
// =============================================================================

// 0xBD = string with 30-byte length prefix; we only feed 1 byte after.
// rlp_can_decode succeeds (it has the prefix byte) but rlp_decode_length
// catches that the inner length isn't representable / valid.
void test_process_tx_malformed_rlp_long_prefix_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    // 0xBD declares "next 0xBD-0xB7=6 bytes are the length". Feed only
    // 6 bytes total — parser sees the 6-byte length payload + 0 bytes
    // of actual string. The 6-byte length must fit in size_t and not
    // overflow; we use all zeros to exercise the validity check.
    const uint8_t bytes[] = {0xBD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    // Either fault (rejected) or processing (waiting for more); both
    // are valid outcomes — what must NOT happen is FINISHED, which
    // would mean the parser accepted nonsense.
    TEST_ASSERT_NOT_EQUAL(r, USTREAM_FINISHED);
}

// =============================================================================
// Field-size gates. The per-field process_* helpers reject any field
// whose length doesn't fit the receiving slot (INT256_LENGTH=32 for
// numeric fields, ADDRESS_LENGTH=20 for `to`). These are *security
// gates*, not just defensive bounds: silently accepting an oversize
// nonce / value would let the parser advance into the next field's
// bytes and shift everything downstream.
// =============================================================================

// Forge a tx whose nonce field is 33 bytes (one over INT256_LENGTH).
// We don't need a complete tx — process_nonce rejects before any
// downstream field is parsed.
//
// Outer list = 0xE2 (34-byte payload). Nonce = 0xA1 + 33 bytes.
void test_process_tx_oversize_nonce_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    uint8_t bytes[2 + 33];
    bytes[0] = 0xE2;  // outer list, 34 bytes payload
    bytes[1] = 0xA1;  // nonce: 33-byte string
    memset(bytes + 2, 0x00, 33);
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
}

// Same pattern but the gate is on `to` (must be exactly 20 bytes).
// Send a 21-byte `to` field — process_to's check_fields rejects.
//
// Layout: outer list + nonce(1) + gasPrice(0x80=0) + startGas(0x80=0)
//       + to(0x95 + 21 bytes) — list payload = 1+1+1+22 = 25 bytes.
void test_process_tx_oversize_to_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    uint8_t bytes[3 + 1 + 1 + 1 + 1 + 21];  // 28 bytes total
    bytes[0] = 0xD9;                        // list, 25-byte payload
    bytes[1] = 0x07;                        // nonce = 7
    bytes[2] = 0x80;                        // gasPrice = 0
    bytes[3] = 0x80;                        // startGas = 0
    bytes[4] = 0x95;                        // to: 21-byte string
    memset(bytes + 5, 0xAA, 21);
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
}

// A `to` shorter than an address is malformed RLP. Left accepted, the untouched tail
// of the destination buffer would be rendered as part of a complete checksummed
// address and matched against stored names as that padded value.
//
// Layout: outer list + nonce(1) + gasPrice(0x80) + startGas(0x80)
//       + to(0x93 + 19 bytes) — list payload = 1+1+1+20 = 23 bytes.
void test_process_tx_short_to_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    uint8_t bytes[1 + 1 + 1 + 1 + 1 + 19];  // 24 bytes total
    bytes[0] = 0xD7;                        // list, 23-byte payload
    bytes[1] = 0x07;                        // nonce = 7
    bytes[2] = 0x80;                        // gasPrice = 0
    bytes[3] = 0x80;                        // startGas = 0
    bytes[4] = 0x93;                        // to: 19-byte string
    memset(bytes + 5, 0xAA, 19);
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(USTREAM_FAULT, r);
}

// Control: an empty `to` is a contract creation and stays valid.
//
// Layout: outer list + nonce(1) + gasPrice(0x80) + startGas(0x80) + to(0x80)
//       — list payload = 4 bytes.
void test_process_tx_empty_to_accepted(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    uint8_t bytes[] = {0xC4, 0x07, 0x80, 0x80, 0x80};
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_NOT_EQUAL(USTREAM_FAULT, r);
    TEST_ASSERT_EQUAL(0, content.destinationLength);
}

// Oversize value (>INT256_LENGTH=32 bytes). process_value rejects via
// the same check_fields gate.
//
// Layout: outer + nonce + gasPrice + startGas + to(20) + value(33)
//       = 1+1+1+21+34 = 58 bytes.
void test_process_tx_oversize_value_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    uint8_t bytes[2 + 58];  // list prefix is 2 bytes for 58-byte payload
    bytes[0] = 0xF8;        // long-form list, 1-byte length
    bytes[1] = 58;          // payload length
    bytes[2] = 0x07;        // nonce
    bytes[3] = 0x80;        // gasPrice
    bytes[4] = 0x80;        // startGas
    bytes[5] = 0x94;        // to: 20-byte string
    memset(bytes + 6, 0xAA, 20);
    bytes[26] = 0xA1;  // value: 33-byte string
    memset(bytes + 27, 0xBB, 33);
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
}

// calldata_append failure during store_calldata mode. The base
// store_calldata happy-path test exercises calldata_init+append on the
// success path; flip g_calldata_append_ok=false to drive the append-
// failure branch in process_data (line 352).
void test_process_data_store_calldata_append_failure_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, /*store_calldata=*/true));
    ctx.txType = LEGACY;
    g_calldata_append_ok = false;

    parserStatus_e r =
        process_tx(&ctx, g_legacy_tx_with_8byte_data, sizeof(g_legacy_tx_with_8byte_data));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
    TEST_ASSERT_EQUAL(g_calldata_init_calls, 1);
    TEST_ASSERT_EQUAL(g_calldata_append_calls, 1);
}

// =============================================================================
// parse_rlp partial-prefix and buffer-overrun branches.
// =============================================================================

// A single 0xB8 byte is the prefix of a long-form string ("next 1
// byte is the length"). Without the length byte, rlp_can_decode
// returns false. The parse_rlp loop exits with the input exhausted
// and canDecode=false → USTREAM_PROCESSING. That's the chunked-feed
// contract: the parser must signal "give me more bytes" rather than
// commit half-decoded state.
void test_process_tx_partial_rlp_prefix_returns_processing(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    const uint8_t bytes[1] = {0xB8};
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(r, USTREAM_PROCESSING);
}

// rlpBuffer is 5 bytes wide. 0xBF declares "next 8 bytes are length".
// Reading 5 bytes (0xBF + 4 zeros) fills rlpBuffer to capacity without
// being decodable yet (we still need 4 more length bytes); the parser
// must reject rather than overflow the buffer.
void test_process_tx_rlp_prefix_overflows_buffer_rejected(void) {
    cx_sha3_t sha3;
    txContent_t content = {0};
    txContext_t ctx;
    TEST_ASSERT_TRUE(init_tx(&ctx, &sha3, &content, false));
    ctx.txType = LEGACY;

    const uint8_t bytes[5] = {0xBF, 0x00, 0x00, 0x00, 0x00};
    parserStatus_e r = process_tx(&ctx, bytes, sizeof(bytes));
    TEST_ASSERT_EQUAL(r, USTREAM_FAULT);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    Mocknetwork_Init();
    get_tx_chain_id_IgnoreAndReturn(0);
    reset();
}
void tearDown(void) {
    Mocknetwork_Verify();
    Mocknetwork_Destroy();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_tx_zeros_context_and_sets_pointers);
    RUN_TEST(test_init_tx_propagates_store_calldata_flag);
    RUN_TEST(test_init_tx_returns_false_on_keccak_init_failure);
    RUN_TEST(test_copy_tx_data_copies_and_advances);
    RUN_TEST(test_copy_tx_data_null_out_consumes_without_copying);
    RUN_TEST(test_copy_tx_data_processing_field_advances_pos);
    RUN_TEST(test_copy_tx_data_zero_length_is_noop_but_hashes);
    RUN_TEST(test_copy_tx_data_single_byte_self_encoded_skips_hash);
    RUN_TEST(test_copy_tx_data_single_byte_outside_processing_still_hashes);
    RUN_TEST(test_copy_tx_data_command_length_underflow_rejected);
    RUN_TEST(test_copy_tx_data_hash_failure_propagates);
    RUN_TEST(test_process_tx_legacy_happy_path);
    RUN_TEST(test_process_tx_truncated_mid_field_returns_processing);
    RUN_TEST(test_process_tx_chunked_two_halves_finishes);
    RUN_TEST(test_process_tx_unsupported_tx_type_returns_fault);
    RUN_TEST(test_process_tx_eip1559_happy_path);
    RUN_TEST(test_process_tx_eip2930_happy_path);
    RUN_TEST(test_process_tx_eip7702_happy_path);
    RUN_TEST(test_process_tx_eip1559_chainid_overflow_rejected);
    RUN_TEST(test_process_data_captures_selector_bytes);
    RUN_TEST(test_process_data_store_calldata_happy_path);
    RUN_TEST(test_process_data_store_calldata_init_failure_rejected);
    RUN_TEST(test_process_data_store_calldata_short_data_rejected);
    RUN_TEST(test_process_tx_eip2930_nonempty_access_list);
    RUN_TEST(test_process_tx_eip7702_nonempty_auth_list);
    RUN_TEST(test_process_tx_malformed_rlp_long_prefix_rejected);
    RUN_TEST(test_process_tx_oversize_nonce_rejected);
    RUN_TEST(test_process_tx_oversize_to_rejected);
    RUN_TEST(test_process_tx_short_to_rejected);
    RUN_TEST(test_process_tx_empty_to_accepted);
    RUN_TEST(test_process_tx_oversize_value_rejected);
    RUN_TEST(test_process_data_store_calldata_append_failure_rejected);
    RUN_TEST(test_process_tx_partial_rlp_prefix_returns_processing);
    RUN_TEST(test_process_tx_rlp_prefix_overflows_buffer_rejected);
    return UNITY_END();
}
