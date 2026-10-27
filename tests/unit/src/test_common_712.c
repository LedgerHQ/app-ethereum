/**
 * @file test_common_712.c
 * @brief Unit tests for the EIP-712 hash pairs at
 *        src/features/sign_message_eip712_common/common_712.c.
 *
 * Behaviors covered:
 *   - eip712_format_hash() fills the "Domain hash" / "Message hash" pairs,
 *   - it fails when the pairs list cannot hold both entries,
 *   - it fails on any allocation failure, leaving both pairs empty and no
 *     hash string allocated.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "common_712.h"
#include "ui_utils.h"
#include "shared_context.h"

// =============================================================================
// Allocator instrumentation (-Wl,--wrap on the app allocator)
// =============================================================================

static int g_live_allocs = 0;
// 0 = never fail, N = fail the N-th allocation from now on
static int g_fail_alloc_at = 0;

bool __real_mem_utils_calloc(void **buffer,
                             size_t size,
                             bool permanent,
                             const char *file,
                             int line);
void __real_mem_utils_free(void *ptr, const char *file, int line);
void __real_mem_utils_free_and_null(void **buffer, const char *file, int line);

bool __wrap_mem_utils_calloc(void **buffer,
                             size_t size,
                             bool permanent,
                             const char *file,
                             int line) {
    if ((g_fail_alloc_at > 0) && (--g_fail_alloc_at == 0)) {
        *buffer = NULL;
        return false;
    }
    bool ret = __real_mem_utils_calloc(buffer, size, permanent, file, line);
    if (ret && (*buffer != NULL)) {
        g_live_allocs++;
    }
    return ret;
}

void __wrap_mem_utils_free(void *ptr, const char *file, int line) {
    if (ptr != NULL) {
        g_live_allocs--;
    }
    __real_mem_utils_free(ptr, file, line);
}

void __wrap_mem_utils_free_and_null(void **buffer, const char *file, int line) {
    if (*buffer != NULL) {
        g_live_allocs--;
    }
    __real_mem_utils_free_and_null(buffer, file, line);
}

// =============================================================================
// Collaborators unreached by these tests
// =============================================================================

void io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    (void) sw;
    (void) tx;
    (void) reset;
    (void) idle;
}

void io_seproxyhal_io_heartbeat(void) {
}

void reset_app_context(void) {
}

cx_err_t cx_hash_no_throw(cx_hash_t *hash,
                          uint32_t mode,
                          const uint8_t *in,
                          size_t in_len,
                          uint8_t *out,
                          size_t out_len) {
    (void) hash;
    (void) mode;
    (void) in;
    (void) in_len;
    (void) out;
    (void) out_len;
    return CX_OK;
}

cx_err_t bip32_derive_with_seed_ecdsa_sign_rs_hash_256(cx_curve_t curve,
                                                       const uint32_t *path,
                                                       size_t path_len,
                                                       uint32_t sign_mode,
                                                       cx_md_t hashID,
                                                       const uint8_t *hash,
                                                       size_t hash_len,
                                                       uint8_t *sig_r,
                                                       uint8_t *sig_s,
                                                       uint32_t *info,
                                                       unsigned int mode,
                                                       unsigned char *seed_key,
                                                       unsigned int seed_key_length) {
    (void) curve;
    (void) path;
    (void) path_len;
    (void) sign_mode;
    (void) hashID;
    (void) hash;
    (void) hash_len;
    (void) sig_r;
    (void) sig_s;
    (void) info;
    (void) mode;
    (void) seed_key;
    (void) seed_key_length;
    return CX_OK;
}

// =============================================================================
// Tests
// =============================================================================

void test_format_hash_fills_both_pairs(void) {
    TEST_ASSERT_TRUE(ui_pairs_init(2));
    TEST_ASSERT_TRUE(eip712_format_hash(0));
    TEST_ASSERT_EQUAL_STRING("Domain hash", g_pairs[0].item);
    TEST_ASSERT_NOT_NULL(g_pairs[0].value);
    TEST_ASSERT_EQUAL_STRING("Message hash", g_pairs[1].item);
    TEST_ASSERT_NOT_NULL(g_pairs[1].value);
}

void test_format_hash_no_room_for_both_pairs_rejected(void) {
    TEST_ASSERT_TRUE(ui_pairs_init(2));
    TEST_ASSERT_FALSE(eip712_format_hash(1));
    TEST_ASSERT_NULL(g_pairs[1].item);
}

void test_format_hash_without_pairs_rejected(void) {
    TEST_ASSERT_FALSE(eip712_format_hash(0));
}

static void check_alloc_failure(int fail_at) {
    TEST_ASSERT_TRUE(ui_pairs_init(2));
    int live = g_live_allocs;
    g_fail_alloc_at = fail_at;
    TEST_ASSERT_FALSE(eip712_format_hash(0));
    TEST_ASSERT_NULL(g_pairs[0].item);
    TEST_ASSERT_NULL(g_pairs[0].value);
    TEST_ASSERT_NULL(g_pairs[1].item);
    TEST_ASSERT_NULL(g_pairs[1].value);
    // No hash string kept behind
    TEST_ASSERT_EQUAL_INT(live, g_live_allocs);
}

void test_format_hash_domain_alloc_failure_rejected(void) {
    check_alloc_failure(1);
}

void test_format_hash_message_alloc_failure_rejected(void) {
    check_alloc_failure(2);
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    g_fail_alloc_at = 0;
    memset(&tmpCtx, 0, sizeof(tmpCtx));
}

void tearDown(void) {
    g_fail_alloc_at = 0;
    eip712_hash_strs_cleanup();
    ui_all_cleanup();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_format_hash_fills_both_pairs);
    RUN_TEST(test_format_hash_no_room_for_both_pairs_rejected);
    RUN_TEST(test_format_hash_without_pairs_rejected);
    RUN_TEST(test_format_hash_domain_alloc_failure_rejected);
    RUN_TEST(test_format_hash_message_alloc_failure_rejected);
    return UNITY_END();
}
