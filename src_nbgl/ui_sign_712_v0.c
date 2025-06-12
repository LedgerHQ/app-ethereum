#include "ui_nbgl.h"
#include "common_712.h"
#include "ui_message_signing.h"
#include "cmd_get_tx_simulation.h"
#include "utils.h"
#include "ui_utils.h"

#define MAX_NB_PAIRS   2
#define FINISH_MSG_LEN 32

void ui_sign_712_v0(void) {
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_EIP712;
    // Initialize the buffers
    if (!ui_pairs_init(MAX_NB_PAIRS)) {
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
    set_tx_simulation_warning(&warning, true, true);
#endif
    warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);

    // Initialize the g_pairs and g_pairsList structures
    eip712_format_hash(g_pairs, MAX_NB_PAIRS, g_pairsList);

    snprintf(g_finishMsg, FINISH_MSG_LEN, "%s typed message?", ui_tx_simulation_finish_str());
    nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                               g_pairsList,
                               &ICON_APP_REVIEW,
                               "Review typed message",
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               ui_typed_message_review_choice_v0);
}
