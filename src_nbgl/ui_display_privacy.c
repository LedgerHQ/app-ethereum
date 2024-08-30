#include "common_ui.h"
#include "ui_signing.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "nbgl_content.h"

static void reviewChoice(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_privacy_ok();
    } else {
        io_seproxyhal_touch_privacy_cancel();
    }
}

static void buildFirstPage(const char *review_string) {
    static nbgl_contentTagValue_t pairs[2] = {0};
    static nbgl_contentTagValueList_t pairsList = {0};

    pairs[0].item = "Address";
    pairs[0].value = strings.common.toAddress;
    pairs[1].item = "Key";
    pairs[1].value = strings.common.fullAmount;
    pairsList.nbPairs = 2;
    pairsList.pairs = pairs;

    nbgl_useCaseReview(TYPE_OPERATION,
                       &pairsList,
                       get_tx_icon(),
                       review_string,
                       NULL,
                       review_string,
                       reviewChoice);
}

void ui_display_privacy_public_key(void) {
    buildFirstPage("Provide public\nprivacy key");
}

void ui_display_privacy_shared_secret(void) {
    buildFirstPage("Provide public\nsecret key");
}
