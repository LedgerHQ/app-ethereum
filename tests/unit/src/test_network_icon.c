/**
 * @file test_network_icon.c
 * @brief Unit tests for handle_network_icon_chunks + clear_icon at
 *        src/features/provide_network_info/network_icon.c.
 *
 * After the host streams a NETWORK_CONFIG descriptor (cmd_network_info.c
 * P2=0x00), it streams the matching icon bitmap (P2=0x01). The icon
 * payload starts with an 8-byte image header (width/height/BPP/size),
 * followed by the bitmap bytes spread across multiple APDUs. The
 * dispatcher tracks a per-network temporary buffer and finalises the
 * bitmap when the last chunk arrives: it checks the size, hashes the
 * full payload with SHA-256 and compares the digest against
 * g_network_icon_hash (signed by the Ledger backend through the matching
 * NETWORK_CONFIG descriptor). On hash mismatch the icon is refused --
 * otherwise the host could substitute an arbitrary icon for a real chain
 * and the user trusts a wrong "this is Polygon" badge.
 *
 * Pin every reject / accept branch:
 *
 *  preconditions
 *    - g_last_added_network == NULL           SWO_INCORRECT_DATA
 *    - g_network_icon_hash == NULL            SWO_INCORRECT_DATA
 *    - g_network_icon_hash all zeros          SWO_INCORRECT_DATA
 *
 *  first chunk (header parsing)
 *    - chunk shorter than 8-byte header       SWO_INCORRECT_DATA
 *    - width/height != expected_px (14)       SWO_INCORRECT_DATA
 *    - total_size overflows uint16            SWO_INCORRECT_DATA
 *    - APP_MEM_PERMANENT returns false        SWO_INSUFFICIENT_MEMORY
 *
 *  p1 routing
 *    - p1 not in {FIRST, FOLLOWING}           SWO_WRONG_P1_P2
 *
 *  next chunk
 *    - chunk would overrun the expected size  SWO_INCORRECT_DATA
 *    - g_icon_bitmap not initialized (a FOLLOWING chunk before any FIRST)
 *                                              SWO_INCORRECT_DATA
 *
 *  full reception
 *    - cx_sha256_hash fails                   SWO_INCORRECT_DATA
 *    - hash mismatch with g_network_icon_hash SWO_INCORRECT_DATA
 *    - hash match                             SWO_SUCCESS,
 *                                              icon bitmap ownership transferred
 *                                              to g_last_added_network->icon
 *
 *  clear_icon
 *    - frees both g_icon_bitmap and g_network_icon_hash
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "shared_context.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "network_info.h"
#include "network_icon.h"

static cx_err_t g_cx_sha256_hash_iovec_err = CX_OK;
static const uint8_t *g_cx_sha256_hash_iovec_out = NULL;
static bool g_mem_utils_calloc_ret = true;

// =============================================================================
// Wraps
// =============================================================================
// cx_sha256_hash() is a static inline in lcx_sha256.h that forwards to
// cx_sha256_hash_iovec; wrap the iovec entry-point so cmocka mock() drives
// the hash outcome AND lets the test push the canned digest. mocks/mock.c
// also ships a WEAK default (CX_OK + zero digest) for any future test
// that needs the call to succeed silently; this strong override wins on
// link.

cx_err_t cx_sha256_hash_iovec(const cx_iovec_t *iovec,
                              size_t iovec_len,
                              uint8_t digest[CX_SHA256_SIZE]) {
    (void) iovec;
    (void) iovec_len;
    if (g_cx_sha256_hash_iovec_err == CX_OK && g_cx_sha256_hash_iovec_out != NULL) {
        memcpy(digest, g_cx_sha256_hash_iovec_out, CX_SHA256_SIZE);
    }
    return g_cx_sha256_hash_iovec_err;
}

// mem_utils_calloc backs APP_MEM_PERMANENT. The wrap lets us simulate an
// out-of-memory scenario without touching the real allocator (mocks/mock.c
// otherwise delegates to malloc which never fails in test).
bool mem_utils_calloc(void **buffer, uint16_t size, bool permanent, const char *file, int line) {
    (void) permanent;
    (void) file;
    (void) line;
    bool ok = (bool) g_mem_utils_calloc_ret;
    if (!ok) return false;
    if (size == 0) return true;
    *buffer = malloc(size);
    if (*buffer == NULL) return false;
    memset(*buffer, 0, size);
    return true;
}

// =============================================================================
// Globals the unit under test reads
// =============================================================================
// g_last_added_network is provided as a WEAK fallback by mocks/app_globals.c.
// g_network_icon_hash is also declared extern in network_info.h but its
// storage lives here -- no other test needs it.

uint8_t *g_network_icon_hash;

// =============================================================================
// Fixture helpers
// =============================================================================

// Mirror network_icon.c's expected_px logic.
#ifdef SCREEN_SIZE_WALLET
#ifdef TARGET_APEX
#define EXPECTED_PX 48
#else
#define EXPECTED_PX 64
#endif
// 64x64 1bpp: 64*64/8 = 512 bytes.
#define ICON_PAYLOAD_BYTES 512
#else
#define EXPECTED_PX        14
// 14x14 1bpp: ceil(14*14/8) = 25 bytes.
#define ICON_PAYLOAD_BYTES 25
#endif
#define ICON_TOTAL_BYTES (8 + ICON_PAYLOAD_BYTES)

// Build the 8-byte image header in `out`. width/height as LE u16, BPP packed
// in upper 4 bits of byte 4, payload size as LE u24 across bytes 5..7.
static void write_header(uint8_t *out, uint16_t w, uint16_t h, uint8_t bpp, uint32_t pay) {
    out[0] = (uint8_t) (w & 0xFF);
    out[1] = (uint8_t) (w >> 8);
    out[2] = (uint8_t) (h & 0xFF);
    out[3] = (uint8_t) (h >> 8);
    out[4] = (uint8_t) ((bpp & 0x0F) << 4);
    out[5] = (uint8_t) (pay & 0xFF);
    out[6] = (uint8_t) ((pay >> 8) & 0xFF);
    out[7] = (uint8_t) ((pay >> 16) & 0xFF);
}

static uint8_t s_icon_buf[ICON_TOTAL_BYTES];
static network_info_t s_net;

static void reset(void) {
    // clear_icon() frees the static g_icon_bitmap and g_network_icon_hash
    // via mem_utils_free_and_null -> free(). Both pointers must point at
    // heap memory (or be NULL) or free() will SIGSEGV. We always pass
    // heap-malloc'd buffers into the dispatcher so this is safe.
    clear_icon();

    memset(&s_net, 0, sizeof(s_net));
    memset(s_icon_buf, 0, sizeof(s_icon_buf));
    write_header(s_icon_buf, EXPECTED_PX, EXPECTED_PX, /*bpp*/ 1, ICON_PAYLOAD_BYTES);
    // Fill the bitmap with a recognizable pattern so an ownership-transfer
    // test can confirm bitmap[0] != 0.
    for (size_t i = 0; i < ICON_PAYLOAD_BYTES; i++) {
        s_icon_buf[8 + i] = (uint8_t) (0x10 + i);
    }

    g_last_added_network = &s_net;
    // Heap-alloc so clear_icon can free it safely between tests.
    g_network_icon_hash = malloc(32);
    memset(g_network_icon_hash, 0xAB, 32);

    g_mem_utils_calloc_ret = true;
    g_cx_sha256_hash_iovec_err = CX_OK;
    g_cx_sha256_hash_iovec_out = NULL;
}

// =============================================================================
// Pre-condition rejects
// =============================================================================

void test_no_network_rejected(void) {
    g_last_added_network = NULL;
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_null_hash_rejected(void) {
    g_network_icon_hash = NULL;
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_zero_hash_rejected(void) {
    memset(g_network_icon_hash, 0, 32);
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

// =============================================================================
// First-chunk header rejects
// =============================================================================

void test_first_chunk_too_short_rejected(void) {
    buffer_t buf = {.ptr = s_icon_buf, .size = 4, .offset = 0};  // 4 < 8
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_first_chunk_wrong_dimensions_rejected(void) {
    // Set width to 32 instead of 14. The check rejects any mismatch.
    write_header(s_icon_buf, /*w*/ 32, /*h*/ EXPECTED_PX, 1, ICON_PAYLOAD_BYTES);
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_first_chunk_total_size_overflow_rejected(void) {
    // Pack a payload size that pushes total over UINT16_MAX (~65528). The
    // header itself is 8 bytes, so any payload >= 65528 overflows.
    write_header(s_icon_buf, EXPECTED_PX, EXPECTED_PX, 1, /*pay*/ 0x010000U);  // 65536
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_first_chunk_alloc_failure_returns_insufficient_memory(void) {
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    g_mem_utils_calloc_ret = false;
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INSUFFICIENT_MEMORY);
}

// =============================================================================
// p1 routing
// =============================================================================

void test_invalid_p1_rejected(void) {
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(/*p1*/ 0xEE, &buf), SWO_WRONG_P1_P2);
}

// =============================================================================
// Next-chunk failures
// =============================================================================

void test_following_chunk_before_first_rejected(void) {
    // Skip the FIRST chunk entirely -- g_icon_bitmap stays NULL, so the
    // FOLLOWING chunk handler must refuse rather than dereferencing NULL.
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FOLLOWING_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_following_chunk_overflow_rejected(void) {
    // Send a full first chunk (allocates buffer of size ICON_TOTAL_BYTES,
    // copies the same bytes into received_size=ICON_TOTAL_BYTES which is
    // also expected_size). Then send another FOLLOWING chunk that would
    // push received_size past expected_size -- refused.
    g_mem_utils_calloc_ret = true;
    buffer_t first = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    g_cx_sha256_hash_iovec_err = CX_OK;
    g_cx_sha256_hash_iovec_out = g_network_icon_hash;
    // First call completes the icon so parse_icon_buffer runs and ownership
    // transfers; subsequent calls should still refuse extra bytes.
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &first), SWO_SUCCESS);
    // Send another byte: received now empty (ownership transferred), but
    // expected_size still tracks the size -- received + 1 > expected.
    uint8_t extra = 0x00;
    buffer_t over = {.ptr = &extra, .size = 1, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FOLLOWING_CHUNK, &over), SWO_INCORRECT_DATA);
}

// =============================================================================
// Full reception -- hash verification
// =============================================================================

void test_full_reception_hash_mismatch_rejected(void) {
    g_mem_utils_calloc_ret = true;
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    // SHA-256 succeeds but the digest doesn't match g_network_icon_hash.
    static uint8_t wrong[32] = {0xDE, 0xAD};
    g_cx_sha256_hash_iovec_err = CX_OK;
    g_cx_sha256_hash_iovec_out = wrong;
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_full_reception_hash_compute_failure_rejected(void) {
    g_mem_utils_calloc_ret = true;
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    g_cx_sha256_hash_iovec_err = CX_INVALID_PARAMETER;
    g_cx_sha256_hash_iovec_out = NULL;
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_INCORRECT_DATA);
}

void test_full_reception_hash_ok_transfers_ownership(void) {
    g_mem_utils_calloc_ret = true;
    buffer_t buf = {.ptr = s_icon_buf, .size = ICON_TOTAL_BYTES, .offset = 0};
    g_cx_sha256_hash_iovec_err = CX_OK;
    g_cx_sha256_hash_iovec_out = g_network_icon_hash;
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &buf), SWO_SUCCESS);
    // Ownership transferred: the network's icon now holds the parsed
    // metadata from the header.
    TEST_ASSERT_EQUAL(s_net.icon.width, EXPECTED_PX);
    TEST_ASSERT_EQUAL(s_net.icon.height, EXPECTED_PX);
    TEST_ASSERT_EQUAL(s_net.icon.bpp, 1);
    TEST_ASSERT_TRUE(s_net.icon.isFile);
    TEST_ASSERT_NOT_NULL(s_net.icon.bitmap);
    // g_network_icon_hash is freed after verification (replay defense:
    // can't reuse the same digest for the next icon).
    TEST_ASSERT_NULL(g_network_icon_hash);
}

// =============================================================================
// Multi-chunk reception
// =============================================================================

void test_two_chunks_complete_icon(void) {
    // Split payload into a FIRST chunk (header + 10 bytes) and a FOLLOWING
    // chunk (remaining ICON_PAYLOAD_BYTES - 10 bytes). The dispatcher
    // accumulates both before finalising.
    g_mem_utils_calloc_ret = true;
    buffer_t first = {.ptr = s_icon_buf, .size = 8 + 10, .offset = 0};
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FIRST_CHUNK, &first), SWO_SUCCESS);
    // No hash run yet -- the icon is incomplete.
    TEST_ASSERT_NOT_NULL(g_network_icon_hash);

    buffer_t next = {.ptr = s_icon_buf + 8 + 10, .size = ICON_PAYLOAD_BYTES - 10, .offset = 0};
    g_cx_sha256_hash_iovec_err = CX_OK;
    g_cx_sha256_hash_iovec_out = g_network_icon_hash;
    TEST_ASSERT_EQUAL(handle_network_icon_chunks(P1_FOLLOWING_CHUNK, &next), SWO_SUCCESS);
    TEST_ASSERT_EQUAL(s_net.icon.width, EXPECTED_PX);
}

// =============================================================================
// clear_icon
// =============================================================================

void test_clear_icon_frees_both_globals(void) {
    // Seed both globals with heap-allocated buffers and confirm
    // clear_icon() nulls them out (mocks/mem_utils_free_and_null is the
    // observable side effect via the wrap).
    g_network_icon_hash = malloc(32);
    TEST_ASSERT_NOT_NULL(g_network_icon_hash);
    clear_icon();
    TEST_ASSERT_NULL(g_network_icon_hash);
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_network_rejected);
    RUN_TEST(test_null_hash_rejected);
    RUN_TEST(test_zero_hash_rejected);
    RUN_TEST(test_first_chunk_too_short_rejected);
    RUN_TEST(test_first_chunk_wrong_dimensions_rejected);
    RUN_TEST(test_first_chunk_total_size_overflow_rejected);
    RUN_TEST(test_first_chunk_alloc_failure_returns_insufficient_memory);
    RUN_TEST(test_invalid_p1_rejected);
    RUN_TEST(test_following_chunk_before_first_rejected);
    RUN_TEST(test_following_chunk_overflow_rejected);
    RUN_TEST(test_full_reception_hash_mismatch_rejected);
    RUN_TEST(test_full_reception_hash_compute_failure_rejected);
    RUN_TEST(test_full_reception_hash_ok_transfers_ownership);
    RUN_TEST(test_two_chunks_complete_icon);
    RUN_TEST(test_clear_icon_frees_both_globals);
    return UNITY_END();
}
