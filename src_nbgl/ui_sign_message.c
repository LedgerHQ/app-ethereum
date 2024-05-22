#include <nbgl_page.h>
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
static nbgl_contentTagValueList_t tagValueList;

static uint32_t eip191MessageIdx = 0;
static uint32_t stringsTmpTmpIdx = 0;

static void reject_message(void) {
    io_seproxyhal_touch_signMessage_cancel();
}

static void sign_message(void) {
    io_seproxyhal_touch_signMessage_ok();
}

static void ui_191_finish_cb(bool confirm) {
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, sign_message);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, reject_message);
    }
}

static void ui_191_skip_cb(void) {
    nbgl_useCaseReviewStreamingFinish(TEXT_SIGN_EIP191, ui_191_finish_cb);
}

static void getTagValues(void) {
    uint16_t len = 0;
    bool reached;

    strncpy(g_stax_shared_buffer + eip191MessageIdx,
            strings.tmp.tmp + stringsTmpTmpIdx,
            SHARED_BUFFER_SIZE - eip191MessageIdx);
    reached = nbgl_getTextMaxLenInNbLines(LARGE_MEDIUM_FONT,
                                          (char *) g_stax_shared_buffer,
                                          SCREEN_WIDTH - (2 * BORDER_MARGIN),
                                          NB_MAX_LINES_IN_REVIEW,
                                          &len,
                                          false);

    stringsTmpTmpIdx = len - eip191MessageIdx;
    eip191MessageIdx = len;
    g_stax_shared_buffer[eip191MessageIdx] = '\0';

    if (!reached && eip191MessageIdx < SHARED_BUFFER_SIZE) {
        stringsTmpTmpIdx = 0;
        question_switcher();
    } else if (reached || eip191MessageIdx == SHARED_BUFFER_SIZE) {
        eip191MessageIdx = 0;
    }

    pair.value = g_stax_shared_buffer;
    pair.item = "Message";
    tagValueList.nbPairs = 1;
    tagValueList.pairs = &pair;
    tagValueList.smallCaseForValue = false;
    tagValueList.nbMaxLinesForValue = NB_MAX_LINES_IN_REVIEW;
    tagValueList.wrapping = false;
}

static void ui_191_data_cb(bool more) {
    if (more) {
        switch (g_action) {
            case UI_191_ACTION_IDLE:
                g_action = UI_191_ACTION_ADVANCE_IN_MESSAGE;
                __attribute__((fallthrough));
            case UI_191_ACTION_ADVANCE_IN_MESSAGE:
                getTagValues();
                nbgl_useCaseReviewStreamingContinueExt(&tagValueList,
                                                       ui_191_data_cb,
                                                       ui_191_skip_cb);
                break;
            case UI_191_ACTION_GO_TO_SIGN:
                nbgl_useCaseReviewStreamingFinish(TEXT_SIGN_EIP191, ui_191_finish_cb);
                break;
        }
    } else {
        ui_191_finish_cb(false);
    }
}

void ui_191_start(void) {
    g_action = UI_191_ACTION_IDLE;
    eip191MessageIdx = 0;
    stringsTmpTmpIdx = 0;

    nbgl_useCaseReviewStreamingStart(TYPE_MESSAGE | SKIPPABLE_OPERATION,
                                     &C_Review_64px,
                                     TEXT_REVIEW_EIP191,
                                     NULL,
                                     ui_191_data_cb);
}

void ui_191_switch_to_message(void) {
    // Get following part of the message
    getTagValues();
}

void ui_191_switch_to_sign(void) {
    g_action = UI_191_ACTION_GO_TO_SIGN;
    // Next callback must display the hold to approve screen
}

void ui_191_switch_to_question(void) {
    // No question mechanism on Stax: Always display the next message chunk.
    continue_displaying_message();
}
