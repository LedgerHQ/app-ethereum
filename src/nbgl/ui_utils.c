#include "nbgl_use_case.h"
#include "app_mem_utils.h"
#include "apdu_constants.h"
#include "common_ui.h"
#include "ui_utils.h"
#include "mem_utils.h"

nbgl_contentTagValue_t *g_pairs = NULL;
nbgl_contentTagValueList_t *g_pairsList = NULL;

// Set by a command that has displayed an error screen of its own, so the main loop
// answers the host without replacing it with the home screen.
static bool s_error_screen_held;

char *g_titleMsg = NULL;
char *g_subTitleMsg = NULL;
char *g_finishMsg = NULL;

/**
 * Internal Cleanup to free allocated memory and send an error status
 */
static void _cleanup(void) {
    ui_all_cleanup();
    io_seproxyhal_send_status(SWO_INSUFFICIENT_MEMORY, 0, true, true);
}

void ui_pairs_cleanup(void) {
    // Zero out the list header before freeing the backing array so that any
    // NBGL render callback still in flight sees an empty list rather than a
    // dangling pairs pointer.
    if (g_pairsList != NULL) {
        g_pairsList->pairs = NULL;
        g_pairsList->nbPairs = 0;
    }
    APP_MEM_FREE_AND_NULL((void **) &g_pairs);
    APP_MEM_FREE_AND_NULL((void **) &g_pairsList);
}

void ui_buffers_cleanup(void) {
    APP_MEM_FREE_AND_NULL((void **) &g_titleMsg);
    APP_MEM_FREE_AND_NULL((void **) &g_subTitleMsg);
    APP_MEM_FREE_AND_NULL((void **) &g_finishMsg);
}

void ui_all_cleanup(void) {
    ui_pairs_cleanup();
    ui_buffers_cleanup();
}

/**
 * Initialize the buffers
 *
 * @return whether the initialization was successful
 */
bool ui_pairs_init(size_t nbPairs) {
    if (nbPairs > UINT8_MAX) {
        return false;
    }
    ui_pairs_cleanup();
    // Allocate the pairsList memory
    if (!APP_MEM_CALLOC((void **) &g_pairsList, sizeof(*g_pairsList))) {
        goto error;
    }

    // Allocate the pairs memory
    if (!APP_MEM_CALLOC((void **) &g_pairs, nbPairs * sizeof(*g_pairs))) {
        goto error;
    }
    g_pairsList->nbPairs = nbPairs;
    g_pairsList->pairs = g_pairs;
    g_pairsList->wrapping = true;
    return true;
error:
    _cleanup();
    return false;
}

/**
 * Initialize the buffers
 *
 * @param title_len Length of the Title message buffer
 * @param subtitle_len Length of the SubTitle message buffer
 * @param finish_len Length of the Finish message buffer
 * @return whether the initialization was successful
 */
bool ui_buffers_init(uint8_t title_len, uint8_t subtitle_len, uint8_t finish_len) {
    ui_buffers_cleanup();
    if (title_len > 0) {
        // Allocate the Title message buffer
        if (!APP_MEM_CALLOC((void **) &g_titleMsg, title_len)) {
            goto error;
        }
    }
    if (subtitle_len > 0) {
        // Allocate the SubTitle message buffer
        if (!APP_MEM_CALLOC((void **) &g_subTitleMsg, subtitle_len)) {
            goto error;
        }
    }
    if (finish_len > 0) {
        // Allocate the Finish message buffer
        if (!APP_MEM_CALLOC((void **) &g_finishMsg, finish_len)) {
            goto error;
        }
    }

    return true;
error:
    _cleanup();
    return false;
}

void ui_hold_error_screen(void) {
    s_error_screen_held = true;
}

bool ui_error_screen_held(void) {
    bool held = s_error_screen_held;

    s_error_screen_held = false;
    return held;
}
