/**
 * @file test_hash_bytes.c
 * @brief Unit tests for hash_nbytes / hash_byte / finalize_hash at
 *        src/hash_bytes.c.
 *
 * Thin wrappers over cx_hash_no_throw that the descriptor-verification
 * layer uses (proxy / trusted-name / network / safe / enum-value): all
 * of them feed bytes into a running SHA-256 context, then finalise to
 * the digest that check_signature_with_pubkey verifies against the
 * Ledger backend signature. Failure to update the running hash, or a
 * silent finalize_hash() return value, would let a forged descriptor
 * pass the signature gate.
 *
 * Pin:
 *  - hash_nbytes forwards (in, n, hash_ctx) to cx_hash_no_throw with mode=0
 *  - hash_byte routes through hash_nbytes (size 1)
 *  - finalize_hash returns true on CX_OK, false otherwise; CX_LAST flag
 *    is set so cx_hash_no_throw emits the final digest
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "hash_bytes.h"

// =============================================================================
// Wraps
// =============================================================================
// Capture mode + in + len so the tests can assert the exact argument shape
// the wrapper forwards.

static struct {
    int calls;
    uint32_t last_mode;
    const uint8_t *last_in;
    uint8_t last_in_val; /* copy of in[0] captured before caller's stack unwinds */
    size_t last_in_len;
    uint8_t *last_out;
    size_t last_out_len;
} g_cap;

static cx_err_t g_cx_hash_ret = CX_OK;

cx_err_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t in_len,
                          uint8_t *out,
                          size_t out_len) {
    (void) hash;
    g_cap.calls++;
    g_cap.last_mode = mode;
    g_cap.last_in = in;
    g_cap.last_in_val = (in != NULL && in_len > 0) ? in[0] : 0;
    g_cap.last_in_len = in_len;
    g_cap.last_out = out;
    g_cap.last_out_len = out_len;
    return g_cx_hash_ret;
}

static void reset(void) {
    memset(&g_cap, 0, sizeof(g_cap));
    g_cx_hash_ret = CX_OK;
}

// =============================================================================
// hash_nbytes
// =============================================================================

void test_hash_nbytes_forwards_arguments_with_mode_zero(void) {
    reset();
    uint8_t data[5] = {1, 2, 3, 4, 5};
    g_cx_hash_ret = CX_OK;
    hash_nbytes(data, sizeof(data), (cx_hash_t *) 0xDEAD);
    TEST_ASSERT_EQUAL(g_cap.calls, 1);
    // mode == 0 (CX_HASH_DEFAULT) -- not a finalising call.
    TEST_ASSERT_EQUAL(g_cap.last_mode, 0);
    TEST_ASSERT_EQUAL_PTR(g_cap.last_in, data);
    TEST_ASSERT_EQUAL(g_cap.last_in_len, sizeof(data));
    TEST_ASSERT_NULL(g_cap.last_out);
    TEST_ASSERT_EQUAL(g_cap.last_out_len, 0);
}

// =============================================================================
// hash_byte
// =============================================================================

void test_hash_byte_passes_single_byte_with_len_one(void) {
    reset();
    g_cx_hash_ret = CX_OK;
    hash_byte(0x42, (cx_hash_t *) 0xBEEF);
    TEST_ASSERT_EQUAL(g_cap.calls, 1);
    TEST_ASSERT_EQUAL(g_cap.last_in_len, 1);
    // hash_byte forwards the byte by address-of a local; the value must
    // round-trip correctly.
    TEST_ASSERT_NOT_NULL(g_cap.last_in);
    TEST_ASSERT_EQUAL(0x42, g_cap.last_in_val);
}

// =============================================================================
// finalize_hash
// =============================================================================

void test_finalize_hash_success_returns_true_with_cx_last_flag(void) {
    reset();
    uint8_t digest[32] = {0};
    g_cx_hash_ret = CX_OK;
    TEST_ASSERT_TRUE(finalize_hash((cx_hash_t *) 0xCAFE, digest, sizeof(digest)));
    // CX_LAST flag must be set on the underlying call so the SDK emits
    // the final digest rather than treating this as another update.
    TEST_ASSERT_EQUAL(g_cap.last_mode, CX_LAST);
    TEST_ASSERT_EQUAL_PTR(g_cap.last_out, digest);
    TEST_ASSERT_EQUAL(g_cap.last_out_len, sizeof(digest));
}

void test_finalize_hash_failure_returns_false(void) {
    reset();
    uint8_t digest[32] = {0};
    g_cx_hash_ret = CX_INVALID_PARAMETER;
    TEST_ASSERT_FALSE(finalize_hash((cx_hash_t *) 0xCAFE, digest, sizeof(digest)));
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hash_nbytes_forwards_arguments_with_mode_zero);
    RUN_TEST(test_hash_byte_passes_single_byte_with_len_one);
    RUN_TEST(test_finalize_hash_success_returns_true_with_cx_last_flag);
    RUN_TEST(test_finalize_hash_failure_returns_false);
    return UNITY_END();
}
