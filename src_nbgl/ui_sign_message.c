#include <nbgl_page.h>
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "sign_message.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "common_ui.h"

typedef enum {
    UI_191_NBGL_START_REVIEW_DISPLAYED = 0,
    UI_191_NBGL_GO_TO_NEXT_CONTENT,
    UI_191_NBGL_BACK_FROM_REJECT_CANCEL,
    UI_191_NBGL_GO_TO_SIGN,
    UI_191_NBGL_SIGN_DISPLAYED,
} e_ui_nbgl_191_state;

static e_ui_nbgl_191_state state;
static e_ui_nbgl_191_state state_before_reject_cancel;

static nbgl_layoutTagValue_t pair;
#define MAX_CHARS_PER_PAGE 128+1
static char eip191Message[MAX_CHARS_PER_PAGE] = {0};
static uint32_t eip191MessageIdx = 0;

static void reject_message(void) {
    io_seproxyhal_touch_signMessage_cancel();
}

static void sign_message() {
    io_seproxyhal_touch_signMessage_ok();
}

static bool nav_callback(uint8_t page, nbgl_pageContent_t *content) {
    UNUSED(page);
    if ((state != UI_191_NBGL_GO_TO_SIGN) && (state != UI_191_NBGL_SIGN_DISPLAYED)) {
        if (state != UI_191_NBGL_BACK_FROM_REJECT_CANCEL) {
            memset(eip191Message, 0, sizeof(eip191Message));
            memcpy(eip191Message, strings.tmp.tmp + eip191MessageIdx, MAX_CHARS_PER_PAGE - 1);
            eip191MessageIdx += MAX_CHARS_PER_PAGE - 1;
        }

        pair.value = eip191Message;
        pair.item = "Message";
        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = 1;
        content->tagValueList.pairs = &pair;
        content->tagValueList.smallCaseForValue = false;
        content->tagValueList.nbMaxLinesForValue = 9;
        content->tagValueList.wrapping = true;

        if (state == UI_191_NBGL_BACK_FROM_REJECT_CANCEL) {
            // We come back from Reject screen.
            // The previously displayed content must be redisplayed.
            // Do not call question_switcher() to avoid replacing
            // string.tmp.tmp content.
            state = state_before_reject_cancel;
        } else if (eip191MessageIdx >= strlen(strings.tmp.tmp)) {
            // Fetch the next content to display into strings.tmp.tmp buffer.
            question_switcher();
            eip191MessageIdx = 0;
        }
    } else {
        eip191MessageIdx = 0;
        // the last page must contain a long press button
        content->type = INFO_LONG_PRESS, content->infoLongPress.icon = &C_Message_64px;
        content->infoLongPress.text = "Sign Message?";
        content->infoLongPress.longPressText = "Hold to sign";
        state = UI_191_NBGL_SIGN_DISPLAYED;
    }
    return true;
}

static void choice_callback(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("MESSAGE\nSIGNED", true, sign_message);
        sign_message();
    }
}

static void continue_review(void) {
    nbgl_useCaseForwardOnlyReview("Reject", NULL, nav_callback, choice_callback);
}

static void confirm_transaction_rejection_choice(bool confirm) {
    if (confirm) {
        reject_message();
    } else {
        // Go to previous screen accordingly
        if (state == UI_191_NBGL_START_REVIEW_DISPLAYED) {
            ui_191_start();
        } else {
            if (state != UI_191_NBGL_SIGN_DISPLAYED) {
                state_before_reject_cancel = state;
                state = UI_191_NBGL_BACK_FROM_REJECT_CANCEL;
            }
            continue_review();
        }
    }
}

static void confirm_transaction_rejection() {
    nbgl_useCaseChoice(&C_warning64px,
                       "Reject message?",
                       NULL,
                       "Yes, Reject",
                       "Go back to message",
                       confirm_transaction_rejection_choice);
}

void ui_191_start(void) {
    state = UI_191_NBGL_START_REVIEW_DISPLAYED;
    nbgl_useCaseReviewStart(&C_Message_64px,
                            "Review message",
                            NULL,
                            "Reject",
                            continue_review,
                            confirm_transaction_rejection);
}

void ui_191_switch_to_message(void) {
    // No question mechanism on Stax:
    // Message is already displayed
    state = UI_191_NBGL_GO_TO_NEXT_CONTENT;
}

void ui_191_switch_to_sign(void) {
    // Next nav_callback callback must display
    // the hold to approve screen
    state = UI_191_NBGL_GO_TO_SIGN;
}

void ui_191_switch_to_question(void) {
    // No question mechanism on Stax:
    // Always display the next message chunk.
    continue_displaying_message();
}