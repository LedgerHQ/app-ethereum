#include "apdu_constants.h"
#include "common_ui.h"
#include "common_712.h"
#include "ui_nbgl.h"
#include "ui_message_signing.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"
#include "mem_utils.h"
#include "i18n/i18n.h"

/**
 * Trigger the EIP712 review flow
 *
 * @param operationType the type of operation to review
 * @param choiceCallback the callback to call when the user makes a choice
 *
 */
static void ui_712_start_review(nbgl_operationType_t operationType,
                                nbgl_choiceCallback_t choiceCallback) {
#ifdef SCREEN_SIZE_WALLET
    const char *tx_check_str = ui_tx_simulation_finish_str();
    const char *title_suffix = " typed message?";
#else
    const char *tx_check_str = "Sign";
    const char *title_suffix = " message";
#endif
    uint8_t finish_len = 1;  // Initialize lengths to 1 for '\0' character

    // Initialize the finish title string
    finish_len += strlen(tx_check_str);
    finish_len += strlen(title_suffix);
    if (!ui_buffers_init(0, 0, finish_len)) {
        return;
    }
    snprintf(g_finishMsg, finish_len, "%s%s", tx_check_str, title_suffix);
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif

    nbgl_useCaseAdvancedReview(operationType,
                               g_pairsList,
                               &ICON_APP_REVIEW,
                               STR(REVIEW_TYPED_MESSAGE),
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               choiceCallback);
}

/**
 * Start EIP712 signature review flow
 *
 */
void ui_sign_712(void) {
    // Initialize the pairs list
    ui_712_push_pairs();

    ui_712_start_review(TYPE_MESSAGE, ui_typed_message_review_choice);
}

/**
 * Start EIP712 signature review flow in Legacy (v0) mode
 *
 */
void ui_sign_712_v0(void) {
    ui_712_start(EIP712_FILTERING_BASIC);

    // Initialize the buffers
    if (!ui_pairs_init(2)) {
        // Initialization failed, cleanup and return
        return;
    }

    // Initialize the tag/value pairs
    eip712_format_hash(0);

    ui_712_start_review(TYPE_TRANSACTION, ui_typed_message_review_choice_v0);
}
