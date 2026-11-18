#include "apdu_constants.h"
#include "common_ui.h"
#include "common_712.h"
#include "ui_nbgl.h"
#include "ui_sign_712.h"
#include "ui_icons.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"
#include "mem_utils.h"
#include "cmd_get_gating.h"
#include "utils.h"  // SET_BIT

void ui_712_review_choice(bool confirm) {
    if (confirm) {
        ui_712_approve_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_SIGNED, ui_idle);
    } else {
        ui_712_reject_cb();
        nbgl_useCaseReviewStatus(STATUS_TYPE_MESSAGE_REJECTED, ui_idle);
    }
}

/**
 * @brief Whether the user can skip to the end of the review of a typed message
 *
 * @return whether the review has a skip option
 */
static bool is_review_skippable(void) {
    // In verbose mode, we allow skipping
    if (N_storage.verbose_eip712) {
        return true;
    }
    // A raw message is long, and has the skip option on touch devices
#ifdef SCREEN_SIZE_WALLET
    return (warning.predefinedSet & SET_BIT(BLIND_SIGNING_WARN)) != 0;
#else
    return false;
#endif
}

bool ui_712_start_review(nbgl_operationType_t operationType, nbgl_choiceCallback_t choiceCallback) {
#ifdef HAVE_TRANSACTION_CHECKS
    set_tx_simulation_warning();
#endif
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
        return false;
    }
    snprintf(g_finishMsg, finish_len, "%s%s", tx_check_str, title_suffix);

    if (is_review_skippable()) {
        operationType |= SKIPPABLE_OPERATION;
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

#ifdef HAVE_EIP712_V1
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

    return ui_712_start_review(TYPE_MESSAGE, ui_712_review_choice);
}
#endif  // HAVE_EIP712_V1

/**
 * @brief Start EIP712 signature review flow in Legacy (v0) mode
 * @return boolean indicating success or failure
 */
bool ui_sign_712_v0(void) {
    if (!ui_712_prepare_blind_review()) {
        return false;
    }

    // Initialize the buffers
    if (!ui_pairs_init(1 + EIP712_HASH_ROWS)) {
        // Initialization failed, cleanup and return
        return false;
    }

    // Initialize the tag/value pairs
    if (!eip712_set_from_pair(&g_pairs[0]) || !eip712_format_hash(1)) {
        return false;
    }

    return ui_712_start_review(TYPE_TRANSACTION, ui_712_review_choice);
}
