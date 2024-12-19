#include "nbgl_page.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "sign_message.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "common_ui.h"
#include "ui_message_signing.h"
#include "ui_signing.h"

#define TEXT_REVIEW_EIP191 REVIEW(TEXT_MESSAGE)
#define TEXT_SIGN_EIP191   SIGN(TEXT_MESSAGE)

typedef enum {
    UI_191_ACTION_IDLE = 0,
    UI_191_ACTION_ADVANCE_IN_MESSAGE,
    UI_191_ACTION_GO_TO_SIGN
} e_ui_191_action;

static e_ui_191_action g_action;

static nbgl_contentTagValue_t pair;
static nbgl_contentTagValueList_t pairs_list;

static uint32_t g_display_buffer_idx;
static uint32_t g_rcv_buffer_idx;
static bool g_skipped;

static void ui_191_process_state(void);

static void ui_191_finish_cb(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_signMessage_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_signMessage_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
}

static void ui_191_skip_cb(void) {
    g_skipped = true;
    skip_rest_of_message();
}

static bool ui_191_update_display_buffer(void) {
    uint16_t len = 0;
    bool reached;

    g_stax_shared_buffer[g_display_buffer_idx] = '\0';
    strlcat(g_stax_shared_buffer + g_display_buffer_idx,
            strings.tmp.tmp + g_rcv_buffer_idx,
            sizeof(g_stax_shared_buffer) - g_display_buffer_idx);
    reached = nbgl_getTextMaxLenInNbLines(LARGE_MEDIUM_FONT,
                                          (char *) g_stax_shared_buffer,
                                          SCREEN_WIDTH - (2 * BORDER_MARGIN),
                                          NB_MAX_LINES_IN_REVIEW,
                                          &len,
                                          false);

    g_rcv_buffer_idx += (len - g_display_buffer_idx);
    g_display_buffer_idx = len;
    g_stax_shared_buffer[g_display_buffer_idx] = '\0';

    if (!reached) {
        g_rcv_buffer_idx = 0;
        question_switcher();
        return false;
    }
    g_display_buffer_idx = 0;
    return true;
}

static void ui_191_data_cb(bool more) {
    if (more) {
        ui_191_process_state();
    } else {
        ui_191_finish_cb(false);
    }
}

static void ui_191_show_message(void) {
    pair.value = g_stax_shared_buffer;
    pair.item = "Message";
    pairs_list.nbPairs = 1;
    pairs_list.pairs = &pair;
    pairs_list.smallCaseForValue = false;
    pairs_list.nbMaxLinesForValue = NB_MAX_LINES_IN_REVIEW;
    pairs_list.wrapping = false;
    nbgl_useCaseReviewStreamingContinueExt(&pairs_list, ui_191_data_cb, ui_191_skip_cb);
}

static void ui_191_process_state(void) {
    switch (g_action) {
        case UI_191_ACTION_IDLE:
            g_action = UI_191_ACTION_ADVANCE_IN_MESSAGE;
            __attribute__((fallthrough));
        case UI_191_ACTION_ADVANCE_IN_MESSAGE:
            if (ui_191_update_display_buffer()) {
                ui_191_show_message();
            }
            break;
        case UI_191_ACTION_GO_TO_SIGN:
            nbgl_useCaseReviewStreamingFinish(TEXT_SIGN_EIP191, ui_191_finish_cb);
            break;
    }
}

void ui_191_start(void) {
    g_action = UI_191_ACTION_IDLE;
    g_display_buffer_idx = 0;
    g_rcv_buffer_idx = 0;
    g_skipped = false;

    nbgl_useCaseReviewStreamingStart(TYPE_MESSAGE | SKIPPABLE_OPERATION,
                                     &C_Review_64px,
                                     TEXT_REVIEW_EIP191,
                                     NULL,
                                     ui_191_data_cb);
}

void ui_191_switch_to_message(void) {
    // Get following part of the message
    ui_191_process_state();
}

void ui_191_switch_to_sign(void) {
    g_action = UI_191_ACTION_GO_TO_SIGN;
    if (g_skipped) {
        nbgl_useCaseReviewStreamingFinish(TEXT_SIGN_EIP191, ui_191_finish_cb);
    } else if (g_display_buffer_idx > 0) {
        // still on an incomplete display buffer, show it before the last page
        ui_191_show_message();
    }
}

void ui_191_switch_to_question(void) {
    // No question mechanism on Stax: Always display the next message chunk.
    continue_displaying_message();
}
