#include "nbgl_use_case.h"
#include "app_mem_utils.h"
#include "apdu_constants.h"
#include "ui_callbacks.h"
#include "ui_utils.h"
#include "mem_utils.h"

nbgl_contentTagValue_t *g_pairs = NULL;
nbgl_contentTagValueList_t *g_pairsList = NULL;

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
bool ui_pairs_init(uint8_t nbPairs) {
    ui_pairs_cleanup();
    // Allocate the pairsList memory
    if (!APP_MEM_CALLOC((void **) &g_pairsList, sizeof(nbgl_contentTagValueList_t))) {
        goto error;
    }

    // Allocate the pairs memory
    if (!APP_MEM_CALLOC((void **) &g_pairs, nbPairs * sizeof(nbgl_contentTagValueList_t))) {
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
    // Allocate the Title message buffer
    if (!APP_MEM_CALLOC((void **) &g_titleMsg, title_len)) {
        goto error;
    }
    // Allocate the SubTitle message buffer
    if (!APP_MEM_CALLOC((void **) &g_subTitleMsg, subtitle_len)) {
        goto error;
    }
    // Allocate the Finish message buffer
    if (!APP_MEM_CALLOC((void **) &g_finishMsg, finish_len)) {
        goto error;
    }

    return true;
error:
    _cleanup();
    return false;
}
