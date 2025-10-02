#include "nbgl_use_case.h"
#include "apdu_constants.h"
#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "common_ui.h"
#include "ui_utils.h"
#include "i18n/i18n.h"

static void review7702Choice(bool confirm) {
    if (confirm) {
        auth_7702_ok_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_idle);
    } else {
        auth_7702_cancel_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_idle);
    }
    ui_pairs_cleanup();
}

void ui_sign_7702_auth(void) {
    // Initialize the buffers
    if (!ui_pairs_init(3)) {
        // Initialization failed, cleanup and return
        return;
    }

    g_pairs[0].item = STR(ACCOUNT);
    g_pairs[0].value = strings.common.fromAddress;
    g_pairs[1].item = STR(DELEGATE_TO);
    g_pairs[1].value = strings.common.toAddress;
#ifdef SCREEN_SIZE_WALLET
    g_pairs[2].item = STR(DELEGATE_ON_NETWORK);
#else
    g_pairs[2].item = STR(ON_NETWORK);
#endif
    g_pairs[2].value = strings.common.network_name;

    nbgl_useCaseReview(TYPE_OPERATION,
                       g_pairsList,
                       &ICON_APP_REVIEW,
                       STR(REVIEW_AUTH_UPGRADE),
                       NULL,
#ifdef SCREEN_SIZE_WALLET
                       STR(SIGN_AUTH_UPGRADE),
#else
                       STR(SIGN_OPERATION),
#endif
                       review7702Choice);
}
