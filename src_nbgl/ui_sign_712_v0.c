#include "ui_nbgl.h"
#include "common_712.h"
#include "ui_message_signing.h"
#include "cmd_get_tx_simulation.h"
#include "utils.h"

static nbgl_contentTagValue_t pairs[2];
static nbgl_contentTagValueList_t pairs_list;

void ui_sign_712_v0(void) {
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    appState = APP_STATE_SIGNING_EIP712;
    explicit_bzero(&warning, sizeof(nbgl_warning_t));
#ifdef HAVE_WEB3_CHECKS
    set_tx_simulation_warning(&warning, true, true);
#endif
    warning.predefinedSet |= SET_BIT(BLIND_SIGNING_WARN);

    // Initialize the pairs and pairs_list structures
    eip712_format_hash(pairs, ARRAYLEN(pairs), &pairs_list);

    snprintf(g_stax_shared_buffer,
             sizeof(g_stax_shared_buffer),
             "%s typed message?",
             ui_tx_simulation_finish_str());
    nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                               &pairs_list,
                               &ICON_APP_REVIEW,
                               "Review typed message",
                               NULL,
                               g_stax_shared_buffer,
                               NULL,
                               &warning,
                               ui_typed_message_review_choice_v0);
}
