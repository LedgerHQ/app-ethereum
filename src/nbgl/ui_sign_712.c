#include "apdu_constants.h"
#include "common_ui.h"
#include "common_712.h"
#include "ui_nbgl.h"
#include "ui_icons.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"
#include "mem_utils.h"
#include "cmd_get_gating.h"
#include "proxy_info.h"

static void ui_typed_message_review_choice(bool confirm) {
    if (confirm) {
        ui_712_approve_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        ui_712_reject_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
    ui_all_cleanup();
    proxy_cleanup();
}

/**
 * @brief Trigger the EIP712 review flow
 *
 * @param blind whether the values are shown raw, which is blind signing
 * @param operationType the type of operation to review
 * @param choiceCallback the callback to call when the user makes a choice
 * @return whether the review could be started
 */
static bool ui_712_start_review(bool blind,
                                nbgl_operationType_t operationType,
                                nbgl_choiceCallback_t choiceCallback) {
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif
#ifdef SCREEN_SIZE_WALLET
    const char *tx_check_str = ui_tx_simulation_finish_str();
    const char *title_suffix = " typed message?";
#else
    UNUSED(blind);
    const char *tx_check_str = "Sign";
    const char *title_suffix = " message";
#endif
    uint8_t finish_len = 1;  // Initialize lengths to 1 for '\0' character

    // Initialize the finish title string
    finish_len += strlen(tx_check_str);
    finish_len += strlen(title_suffix);
    if (!ui_buffers_init(0, 0, finish_len)) {
        return false;
    }
    snprintf(g_finishMsg, finish_len, "%s%s", tx_check_str, title_suffix);

    // Use review with skip button in case of raw message
#ifdef SCREEN_SIZE_WALLET
    if (blind) {
        operationType |= SKIPPABLE_OPERATION;
    } else
#endif
    {
        if (N_storage.verbose_eip712) {
            // In verbose mode, we allow skipping
            operationType |= SKIPPABLE_OPERATION;
        }
    }

    nbgl_useCaseAdvancedReview(operationType,
                               g_pairsList,
                               &LARGE_REVIEW_ICON,
                               "Review typed message",
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               choiceCallback);
    return true;
}

/**
 * @brief Start EIP712 signature review flow
 *
 * @param filtering_mode the filtering mode to use
 * @return boolean indicating success or failure
 */
bool ui_sign_712_v1(e_eip712_filtering_mode filtering_mode) {
    // Initialize the pairs list
    if (!ui_712_push_pairs()) {
        return false;
    }

    if (filtering_mode == EIP712_FILTERING_BASIC) {
        if (set_gating_warning() == false) {
            return false;
        }
    }

    return ui_712_start_review(filtering_mode == EIP712_FILTERING_BASIC,
                               TYPE_MESSAGE,
                               ui_typed_message_review_choice);
}

/**
 * @brief Start EIP712 signature review flow in Legacy (v0) mode
 * @return boolean indicating success or failure
 */
bool ui_sign_712_v0(void) {
    if (!ui_712_prepare_blind_review()) {
        return false;
    }

    // Initialize the buffers
    if (!ui_pairs_init(2)) {
        // Initialization failed, cleanup and return
        return false;
    }

    // Initialize the tag/value pairs
    if (!eip712_format_hash(0)) {
        return false;
    }

    return ui_712_start_review(true, TYPE_TRANSACTION, ui_typed_message_review_choice);
}
