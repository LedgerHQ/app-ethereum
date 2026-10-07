#include "shared_context.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "nbgl_use_case.h"
#include "common_ui.h"
#include "ui_utils.h"

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

bool ui_sign_7702_revocation(void) {
    int index = 0;

    // Initialize the buffers
    if (!ui_pairs_init(2 + (N_storage.displayNonce ? 1 : 0))) {
        // Initialization failed, cleanup and return
        return false;
    }

    g_pairs[index].item = "Account";
    g_pairs[index++].value = strings.common.fromAddress;

    g_pairs[index].item = "Revoke on network";
    g_pairs[index++].value = strings.common.network_name;

    if (N_storage.displayNonce) {
        g_pairs[index].item = "Nonce";
        g_pairs[index++].value = strings.common.nonce;
    }

    nbgl_useCaseReview(TYPE_OPERATION,
                       g_pairsList,
                       &LARGE_REVIEW_ICON,
                       "Review authorization to revoke smart contract delegation?",
                       NULL,
#ifdef SCREEN_SIZE_WALLET
                       "Sign authorization to revoke smart contract delegation?",
#else
                       "Sign operation",
#endif
                       review7702Choice);
    return true;
}
