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

static nbgl_contentTagValue_t pair;
static nbgl_contentTagValueList_t pairsList;

static uint32_t eip191MessageIdx = 0;
static uint32_t stringsTmpTmpIdx = 0;

static void reject_message(void) {
    io_seproxyhal_touch_signMessage_cancel();
}

static void sign_message(void) {
    io_seproxyhal_touch_signMessage_ok();
}

static void messageReviewChoice_cb(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("MESSAGE\nSIGNED", true, sign_message);
    } else {
        nbgl_useCaseStatus("Message signing\ncancelled", false, reject_message);
    }
}

static void setTagValuePairs(void) {
    uint16_t len = 0;
    bool reached;

    explicit_bzero(&pair, sizeof(pair));
    explicit_bzero(&pairsList, sizeof(pairsList));

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

    pairsList.nbPairs = 1;
    pairsList.pairs = &pair;
    pairsList.smallCaseForValue = false;
    pairsList.nbMaxLinesForValue = NB_MAX_LINES_IN_REVIEW;
    pairsList.wrapping = false;

    if (stringsTmpTmpIdx >= strlen(strings.tmp.tmp)) {
        // Fetch the next content to display into strings.tmp.tmp buffer.
        stringsTmpTmpIdx = 0;
        question_switcher();
    }
}

static void more_data_cb(bool confirm) {
    if (confirm) {
        if (g_position != UI_SIGNING_POSITION_SIGN) {
            setTagValuePairs();
            nbgl_useCaseReviewStreamingContinue(&pairsList, more_data_cb);
        } else {
            // the last page must contain a long press button
            nbgl_useCaseReviewStreamingFinish(SIGN(TEXT_MESSAGE), messageReviewChoice_cb);
        }
    } else {
        reject_message();
    }
}

void ui_191_start(void) {
    g_position = UI_SIGNING_POSITION_START;
    eip191MessageIdx = 0;
    stringsTmpTmpIdx = 0;

    nbgl_useCaseReviewStreamingStart(TYPE_MESSAGE,
                                     &C_Review_64px,
                                     REVIEW(TEXT_MESSAGE),
                                     NULL,
                                     more_data_cb);
}

void ui_191_switch_to_message(void) {
    g_position = UI_SIGNING_POSITION_REVIEW;
    // No question mechanism on Stax:
    // Message is already displayed
    more_data_cb(true);
}

void ui_191_switch_to_sign(void) {
    g_position = UI_SIGNING_POSITION_SIGN;
}

void ui_191_switch_to_question(void) {
    // No question mechanism on Stax:
    // Always display the next message chunk.
    continue_displaying_message();
}
