#include "sign_message.h"
#include "nbgl_use_case.h"
#include "common_ui.h"
#include "ui_message_signing.h"
#include "ui_signing.h"
#include "ledger_assert.h"
#include "mem.h"

#define TEXT_REVIEW_EIP191 REVIEW(TEXT_MESSAGE)
#define TEXT_SIGN_EIP191   SIGN(TEXT_MESSAGE)

#define MAX_PAIRS 10

static bool skip_message;

static nbgl_contentTagValue_t pairs[MAX_PAIRS];
static nbgl_contentTagValueList_t tagValueList;

static uint32_t message_size = 0;
static char *full_message = NULL;

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

static void getTagValues(void) {
    uint16_t len = 0;
    uint16_t nbPairs = 0;
    uint16_t index = 0;

    // Get Max len per page
    nbgl_getTextMaxLenInNbLines(LARGE_MEDIUM_FONT,
                                full_message,
                                SCREEN_WIDTH - (2 * BORDER_MARGIN),
                                NB_MAX_LINES_IN_REVIEW,
                                &len,
                                false);
    // init the messages for Tag/Value
    do {
        LEDGER_ASSERT(nbPairs < MAX_PAIRS,
                      "Too many pages required (%d / %d)!\n",
                      nbPairs,
                      MAX_PAIRS);
        pairs[nbPairs].value = &full_message[index];
        pairs[nbPairs].item = "Message";
        // Shift message to insert '\0' byte to separate the sub-strings
        // ex: string=ABCDEFGHIJKLMNO, len=5 -> ABCDE\0FGHIJ\0KLMNO
        index += len;
        memmove(&full_message[index + 1], &full_message[index], strlen(&full_message[index]));
        full_message[index] = 0;
        index++;
        nbPairs++;
    } while (strlen(&full_message[index]) > 0);

    tagValueList.nbPairs = nbPairs;
    tagValueList.pairs = pairs;
    tagValueList.smallCaseForValue = false;
    tagValueList.nbMaxLinesForValue = NB_MAX_LINES_IN_REVIEW;
    tagValueList.wrapping = false;
}

void ui_191_start(uint32_t msg_size) {
    g_position = UI_SIGNING_POSITION_START;

    skip_message = false;

    // Add more bytes to insert '\0' bytes to split into different messages
    msg_size += MAX_PAIRS;
    full_message = mem_alloc(msg_size);
    LEDGER_ASSERT(full_message, "No suffisant memory!\n");
    message_size = msg_size;
    full_message[0] = 0;
    // Loop until the whole message is received
    do {
        strlcat(full_message, strings.tmp.tmp, message_size);
        continue_displaying_message();
    } while (strlen(strings.tmp.tmp));
    // Get and initialise the Tag/Value pairs
    getTagValues();
    // Start the review
    nbgl_useCaseReview(TYPE_MESSAGE,
                       &tagValueList,
                       &C_Review_64px,
                       TEXT_REVIEW_EIP191,
                       NULL,
                       TEXT_SIGN_EIP191,
                       ui_191_finish_cb);
}

void ui_191_switch_to_message(void) {
    // No question mechanism on Stax:
    // Message is already displayed
}

void ui_191_switch_to_sign(void) {
    // Next nav_callback callback must display
    // the hold to approve screen
    // if (skip_message) {
    //     continue_review();  // to force screen refresh
    // }
}

void ui_191_switch_to_question(void) {
    // No question mechanism on Stax:
    // Always display the next message chunk.
    continue_displaying_message();
}
