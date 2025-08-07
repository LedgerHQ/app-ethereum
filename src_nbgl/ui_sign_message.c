#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"

#define FINISH_MSG_LEN 32
// TODO Re-activate when partners are ready for eip191
#undef HAVE_TRANSACTION_CHECKS

static void ui_191_finish_cb(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_signMessage_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_signMessage_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
    ui_all_cleanup();
}

void ui_191_start(const char *message) {
    // Initialize the buffers
    if (!ui_pairs_init(1)) {
        // Initialization failed, cleanup and return
        return;
    }
    // Initialize the buffers
    if (!ui_buffers_init(0, 0, FINISH_MSG_LEN)) {
        // Initialization failed, cleanup and return
        return;
    }

    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning(&warning, false, true);
#endif

    snprintf(g_finishMsg,
             FINISH_MSG_LEN,
#ifdef SCREEN_SIZE_WALLET
             "%s message?",
#else
             "%s message",
#endif
             ui_tx_simulation_finish_str());

    g_pairsList->wrapping = true;
    g_pairs->item = "Message";
    g_pairs->value = message;

    nbgl_useCaseAdvancedReview(TYPE_MESSAGE | SKIPPABLE_OPERATION,
                               g_pairsList,
                               &ICON_APP_REVIEW,
                               "Review message",
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               ui_191_finish_cb);
}
