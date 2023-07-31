#include "common_ui.h"
#include "ui_signing.h"
#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "nbgl_use_case.h"
#include "network.h"

#ifdef HAVE_STARKWARE

static nbgl_layoutTagValue_t pairs[2];
static char from_account[64];
static char message_hash[64];

static void reviewReject(void) {
    io_seproxyhal_touch_tx_cancel(NULL);
}

static void confirmTransation(void) {
    io_seproxyhal_touch_stark_unsafe_sign_ok(NULL);
}

static void reviewChoice(bool confirm) {
    if (confirm) {
        confirmTransation();
    } else {
        reviewReject();
    }
}

static bool displayTransactionPage(uint8_t page, nbgl_pageContent_t *content) {
    snprintf(from_account, sizeof(from_account), "0x%.*H", 32, dataContext.starkContext.w1);
    snprintf(message_hash, sizeof(message_hash), "0x%.*H", 32, dataContext.starkContext.w2);

    if (page == 0) {
        pairs[0].item = "From Account";
        pairs[0].value = from_account;
        pairs[1].item = "Hash";
        pairs[1].value = message_hash;
        content->type = TAG_VALUE_LIST;
        content->tagValueList.nbPairs = 2;
        content->tagValueList.pairs = (nbgl_layoutTagValue_t *) pairs;
    } else if (page == 1) {
        content->type = INFO_LONG_PRESS, content->infoLongPress.icon = get_app_icon(false);
        content->infoLongPress.text = "Unsafe Stark Sign";
        content->infoLongPress.longPressText = SIGN_BUTTON;
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
    nbgl_useCaseReviewStart(get_app_icon(false),
                            "Unsafe Stark Sign",
                            NULL,
                            REJECT_BUTTON,
                            reviewContinue,
                            reviewReject);
}

void ui_stark_unsafe_sign(void) {
    buildFirstPage();
}

#endif  // HAVE_STARKWARE
