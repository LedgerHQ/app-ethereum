#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "cmd_get_tx_simulation.h"

// TODO Re-activate when partners are ready for eip191
#undef HAVE_WEB3_CHECKS

static nbgl_contentTagValue_t pair;
static nbgl_contentTagValueList_t pairs_list;

static void ui_191_finish_cb(bool confirm) {
    if (confirm) {
        io_seproxyhal_touch_signMessage_ok();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        io_seproxyhal_touch_signMessage_cancel();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
}

void ui_191_start(const char *message) {
    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_WEB3_CHECKS
    set_tx_simulation_warning(&warning, false, true);
#endif
#ifdef SCREEN_SIZE_WALLET
    snprintf(g_stax_shared_buffer,
             sizeof(g_stax_shared_buffer),
#ifdef SCREEN_SIZE_WALLET
             "%s message?",
#else
             "%s message",
#endif
             ui_tx_simulation_finish_str());
#else
    snprintf(g_stax_shared_buffer,
             sizeof(g_stax_shared_buffer),
             "%s message",
             ui_tx_simulation_finish_str());
#endif
    explicit_bzero(&pair, sizeof(pair));
    explicit_bzero(&pairs_list, sizeof(pairs_list));
    pairs_list.nbPairs = 1;
    pairs_list.pairs = &pair;
    pairs_list.wrapping = true;
    pair.item = "Message";
    pair.value = message;

    nbgl_useCaseAdvancedReview(TYPE_MESSAGE | SKIPPABLE_OPERATION,
                               &pairs_list,
                               &ICON_APP_REVIEW,
                               "Review message",
                               NULL,
                               g_stax_shared_buffer,
                               NULL,
                               &warning,
                               ui_191_finish_cb);
}
