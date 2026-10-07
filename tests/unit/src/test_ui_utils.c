/**
 * @file test_ui_utils.c
 * @brief Unit tests for ui_pairs_init / ui_buffers_init / cleanup helpers
 *        at src/nbgl/ui_utils.c.
 *
 * The pair-list + message-buffer scaffolding the review screens read from
 * lives in three globals (g_pairsList / g_pairs / g_titleMsg /
 * g_subTitleMsg / g_finishMsg). Each init helper:
 *   - frees any previously-allocated buffer (one-shot reset)
 *   - allocates fresh storage through APP_MEM_CALLOC
 *   - on alloc failure, _cleanup() releases everything and emits
 *     SWO_INSUFFICIENT_MEMORY to the host via io_seproxyhal_send_status
 *
 * A regression that leaks (skips the cleanup-before-alloc) or that
 * silently returns true on alloc failure leaves the UI rendering from
 * a half-built buffer -- either a crash on first access or a screen
 * with stale content. Pin every branch.
 */

#include "unity.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "nbgl_use_case.h"
#include "status_words.h"
#include "ui_utils.h"

extern nbgl_contentTagValue_t *g_pairs;
extern nbgl_contentTagValueList_t *g_pairsList;
extern char *g_titleMsg;
extern char *g_subTitleMsg;
extern char *g_finishMsg;

// =============================================================================
// Wraps -- drive APP_MEM_CALLOC / free / send_status
// =============================================================================
// mem_utils_calloc has signature:
//   bool mem_utils_calloc(void **ptr_storage, size_t size, bool /*ALLOC_FILE*/,
//                         int /*ALLOC_LINE*/);
// (the macro fills in the trailing __FILE__/__LINE__ args). We capture the
// requested size + simulate failure via the global below.

static int g_alloc_force_failure_after = -1;  // -1 = always succeed
static int g_alloc_calls = 0;
bool mem_utils_calloc(void **ptr_storage,
                      uint16_t size,
                      bool permanent,
                      const char *file,
                      int line) {
    (void) permanent;
    (void) file;
    (void) line;
    g_alloc_calls++;
    if (g_alloc_force_failure_after >= 0 && g_alloc_calls > g_alloc_force_failure_after) {
        return false;
    }
    *ptr_storage = malloc(size);
    if (*ptr_storage == NULL) {
        return false;
    }
    memset(*ptr_storage, 0, size);
    return true;
}

static int g_free_calls = 0;
void mem_utils_free_and_null(void **ptr_storage, const char *file, int line) {
    (void) file;
    (void) line;
    g_free_calls++;
    if (ptr_storage != NULL && *ptr_storage != NULL) {
        free(*ptr_storage);
        *ptr_storage = NULL;
    }
}

static int g_send_status_calls = 0;
static uint16_t g_send_status_last_sw = 0;
uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    (void) tx;
    (void) reset;
    (void) idle;
    g_send_status_calls++;
    g_send_status_last_sw = sw;
    return 0;
}

// =============================================================================
// Fixture
// =============================================================================

static void reset(void) {
    g_alloc_force_failure_after = -1;
    g_alloc_calls = 0;
    g_free_calls = 0;
    g_send_status_calls = 0;
    g_send_status_last_sw = 0;
    // Force a clean slate: anything left from a previous test is freed.
    ui_all_cleanup();
    g_alloc_calls = 0;
    g_free_calls = 0;
}

// =============================================================================
// ui_pairs_init
// =============================================================================

void test_pairs_init_allocates_list_and_pair_array(void) {
    TEST_ASSERT_TRUE(ui_pairs_init(5));
    TEST_ASSERT_NOT_NULL(g_pairsList);
    TEST_ASSERT_NOT_NULL(g_pairs);
    TEST_ASSERT_EQUAL(g_pairsList->nbPairs, 5);
    TEST_ASSERT_EQUAL_PTR(g_pairsList->pairs, g_pairs);
    TEST_ASSERT_TRUE(g_pairsList->wrapping);
}

void test_pairs_init_releases_previous_storage(void) {
    // First init: 3 pairs.
    ui_pairs_init(3);
    g_free_calls = 0;
    // Second init: 5 pairs. The pre-amble cleanup MUST free the previous
    // g_pairsList + g_pairs (one mem_utils_free_and_null each).
    ui_pairs_init(5);
    TEST_ASSERT_EQUAL(g_free_calls, 2);
}

void test_pairs_init_alloc_failure_returns_false_and_signals_status(void) {
    // First alloc (g_pairsList) succeeds, second (g_pairs) fails.
    g_alloc_force_failure_after = 1;
    TEST_ASSERT_FALSE(ui_pairs_init(5));
    // On failure the internal _cleanup() runs, which frees everything
    // AND emits SWO_INSUFFICIENT_MEMORY.
    TEST_ASSERT_EQUAL(g_send_status_calls, 1);
    TEST_ASSERT_EQUAL(g_send_status_last_sw, SWO_INSUFFICIENT_MEMORY);
}

// =============================================================================
// ui_buffers_init
// =============================================================================

void test_buffers_init_allocates_only_requested_buffers(void) {
    // Only title requested; subtitle + finish stay NULL.
    TEST_ASSERT_TRUE(ui_buffers_init(64, 0, 0));
    TEST_ASSERT_NOT_NULL(g_titleMsg);
    TEST_ASSERT_NULL(g_subTitleMsg);
    TEST_ASSERT_NULL(g_finishMsg);
}

void test_buffers_init_allocates_all_three_when_all_nonzero(void) {
    TEST_ASSERT_TRUE(ui_buffers_init(32, 16, 8));
    TEST_ASSERT_NOT_NULL(g_titleMsg);
    TEST_ASSERT_NOT_NULL(g_subTitleMsg);
    TEST_ASSERT_NOT_NULL(g_finishMsg);
}

void test_buffers_init_failure_releases_and_signals(void) {
    // Title alloc succeeds, subtitle fails.
    g_alloc_force_failure_after = 1;
    TEST_ASSERT_FALSE(ui_buffers_init(32, 16, 8));
    TEST_ASSERT_EQUAL(g_send_status_calls, 1);
    TEST_ASSERT_EQUAL(g_send_status_last_sw, SWO_INSUFFICIENT_MEMORY);
    // After cleanup the globals MUST be NULL again (otherwise a later
    // ui_buffers_cleanup would double-free).
    TEST_ASSERT_NULL(g_titleMsg);
    TEST_ASSERT_NULL(g_subTitleMsg);
    TEST_ASSERT_NULL(g_finishMsg);
}

void test_buffers_init_finish_failure_cleans_up(void) {
    // Title + subtitle succeed, finish (third APP_MEM_CALLOC) fails.
    // Covers the last `goto error` branch.
    g_alloc_force_failure_after = 2;
    TEST_ASSERT_FALSE(ui_buffers_init(32, 16, 8));
    TEST_ASSERT_EQUAL(g_send_status_last_sw, SWO_INSUFFICIENT_MEMORY);
    TEST_ASSERT_NULL(g_titleMsg);
    TEST_ASSERT_NULL(g_subTitleMsg);
    TEST_ASSERT_NULL(g_finishMsg);
}

// =============================================================================
// ui_*_cleanup
// =============================================================================

void test_all_cleanup_frees_pairs_and_buffers(void) {
    ui_pairs_init(3);
    ui_buffers_init(32, 16, 8);
    g_free_calls = 0;
    ui_all_cleanup();
    // 2 free for pairs (g_pairs + g_pairsList) + 3 free for buffers
    // (title + subtitle + finish).
    TEST_ASSERT_EQUAL(g_free_calls, 5);
    TEST_ASSERT_NULL(g_pairsList);
    TEST_ASSERT_NULL(g_pairs);
    TEST_ASSERT_NULL(g_titleMsg);
}

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pairs_init_allocates_list_and_pair_array);
    RUN_TEST(test_pairs_init_releases_previous_storage);
    RUN_TEST(test_pairs_init_alloc_failure_returns_false_and_signals_status);
    RUN_TEST(test_buffers_init_allocates_only_requested_buffers);
    RUN_TEST(test_buffers_init_allocates_all_three_when_all_nonzero);
    RUN_TEST(test_buffers_init_failure_releases_and_signals);
    RUN_TEST(test_buffers_init_finish_failure_cleans_up);
    RUN_TEST(test_all_cleanup_frees_pairs_and_buffers);
    return UNITY_END();
}
