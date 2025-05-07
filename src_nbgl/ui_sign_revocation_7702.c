#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "common_ui.h"

static nbgl_contentTagValue_t pairs[2] = {0};
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

void ui_sign_7702_revocation(void) {
    pairs[0].item = "Account";
    pairs[0].value = strings.common.fromAddress;
    pairs[1].item = "Revoke on network";
    pairs[1].value = strings.common.network_name;

    pairsList.nbPairs = ARRAYLEN(pairs);
    pairsList.pairs = pairs;

    nbgl_useCaseReview(TYPE_OPERATION,
                       &pairsList,
                       &ICON_APP_REVIEW,
                       "Review authorization to revoke smart contract delegation?",
                       NULL,
                       "Sign authorization to revoke smart contract delegation?",
                       review7702Choice);
}
