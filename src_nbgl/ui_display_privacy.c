#include "common_ui.h"
#include "ui_signing.h"
#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "nbgl_use_case.h"
#include "network.h"

static nbgl_layoutTagValue_t pairs[2];
static char *review_string;

static void reviewReject(void) {
    io_seproxyhal_touch_privacy_cancel(NULL);
}

static void confirmTransation(void) {
    io_seproxyhal_touch_privacy_ok(NULL);
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        confirmTransation();
    } else {
        reviewReject();
    }
}

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
    if (page == 0) {
        pairs[0].item = "Address";
        pairs[0].value = strings.common.fullAddress;
        pairs[1].item = "Key";
        pairs[1].value = strings.common.fullAmount;

        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = 2;
        content->tagValueList.pairs = (nbgl_layoutTagValue_t *) pairs;
    } else if (page == 1) {
        content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(true);
        content->infoLongPress.text = review_string;
        content->infoLongPress.longPressText = "Hold to approve";
    } else {
        return false;
    }
    // valid page so return true
    return true;
}

static void reviewContinue(void) {
    nbgl_useCaseRegularReview(0, 2, REJECT_BUTTON, NULL, displayTransactionPage, reviewChoice);
}

static void buildFirstPage(void) {
    nbgl_useCaseReviewStart(get_app_icon(true),
                            review_string,
                            NULL,
                            REJECT_BUTTON,
                            reviewContinue,
                            reviewReject);
}

void ui_display_privacy_public_key(void) {
    review_string = (char *) "Provide public\nprivacy key";
    buildFirstPage();
}

void ui_display_privacy_shared_secret(void) {
    review_string = (char *) "Provide public\nsecret key";
    buildFirstPage();
}
