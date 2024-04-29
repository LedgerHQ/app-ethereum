#include "common_ui.h"
#include "ui_signing.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "nbgl_content.h"

enum {
    TOKEN_APPROVE = FIRST_USER_TOKEN,
};

static void reviewReject(void) {
    io_seproxyhal_touch_privacy_cancel(NULL);
}

static void long_press_cb(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);
    if (token == TOKEN_APPROVE) {
        io_seproxyhal_touch_privacy_ok(NULL);
    }
}

static void buildFirstPage(const char *review_string) {
    static nbgl_genericContents_t contents = {0};
    static nbgl_content_t contentsList[3] = {0};
    static nbgl_contentTagValue_t pairs[2] = {0};
    uint8_t nbContents = 0;
    uint8_t nbPairs = 0;

    pairs[nbPairs].item = "Address";
    pairs[nbPairs].value = strings.common.fullAddress;
    nbPairs++;
    pairs[nbPairs].item = "Key";
    pairs[nbPairs].value = strings.common.fullAmount;
    nbPairs++;

    // Title page
    contentsList[nbContents].type = CENTERED_INFO;
    contentsList[nbContents].content.centeredInfo.text1 = review_string;
    contentsList[nbContents].content.centeredInfo.icon = get_app_icon(true);
    contentsList[nbContents].content.centeredInfo.style = LARGE_CASE_INFO;
    nbContents++;

    // Values to be reviewed
    contentsList[nbContents].type = TAG_VALUE_LIST;
    contentsList[nbContents].content.tagValueList.pairs = pairs;
    contentsList[nbContents].content.tagValueList.nbPairs = nbPairs;
    nbContents++;

    // Approval screen
    contentsList[nbContents].type = INFO_LONG_PRESS;
    contentsList[nbContents].content.infoLongPress.text = review_string;
    contentsList[nbContents].content.infoLongPress.icon = get_app_icon(true);
    contentsList[nbContents].content.infoLongPress.longPressText = "Hold to approve";
    contentsList[nbContents].content.infoLongPress.longPressToken = TOKEN_APPROVE;
    contentsList[nbContents].content.infoLongPress.tuneId = NB_TUNES;
    contentsList[nbContents].contentActionCallback = long_press_cb;
    nbContents++;

    contents.callbackCallNeeded = false;
    contents.contentsList = contentsList;
    contents.nbContents = nbContents;

    nbgl_useCaseGenericReview(&contents, REJECT_BUTTON, reviewReject);
}

void ui_display_privacy_public_key(void) {
    buildFirstPage("Provide public\nprivacy key");
}

void ui_display_privacy_shared_secret(void) {
    buildFirstPage("Provide public\nsecret key");
}
