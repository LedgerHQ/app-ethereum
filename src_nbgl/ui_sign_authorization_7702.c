#include "nbgl_page.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "common_ui.h"

static nbgl_contentTagValue_t pairs[4] = {0};
static nbgl_contentTagValueList_t pairsList = {0};

static void review7702Choice(bool confirm) {
    if (confirm) {
        auth_7702_ok_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_idle);
    } else {
        auth_7702_cancel_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_idle);
    }
}

void ui_sign_7702_auth(void) {
    pairs[0].item = "Account";
    pairs[0].value = strings.common.fromAddress;
    pairs[1].item = "Delegate to";
    pairs[1].value = strings.common.toAddress;
    pairs[2].item = "Delegate on network";
    pairs[2].value = strings.common.network_name;

    pairsList.nbPairs = 3;
    pairsList.pairs = pairs;

    nbgl_useCaseReview(TYPE_OPERATION,
                       &pairsList,
                       &ICON_APP_REVIEW,
                       "Review authorization\nto upgrade into smart\ncontract account ?",
                       NULL,
                       "Sign authorization to\nupgrade into smart\ncontract account ?",
                       review7702Choice);
}
