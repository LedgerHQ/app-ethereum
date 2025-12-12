#include "apdu_constants.h"
#include "common_ui.h"
#include "common_712.h"
#include "ui_nbgl.h"
#include "ui_message_signing.h"
#include "cmd_get_tx_simulation.h"
#include "ui_utils.h"
#include "mem_utils.h"

/**
 * @brief Trigger the EIP712 review flow
 *
 * @param filtering_mode the filtering mode to use
 * @param operationType the type of operation to review
 * @param choiceCallback the callback to call when the user makes a choice
 *
 */
static void ui_712_start_review(e_eip712_filtering_mode filtering_mode,
                                nbgl_operationType_t operationType,
                                nbgl_choiceCallback_t choiceCallback) {
#ifdef SCREEN_SIZE_WALLET
    const char *tx_check_str = ui_tx_simulation_finish_str();
    const char *title_suffix = " typed message?";
#else
    UNUSED(filtering_mode);
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

    // Use review with skip button in case of raw message
#ifdef SCREEN_SIZE_WALLET
    if (filtering_mode == EIP712_FILTERING_BASIC) {
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
                               &ICON_APP_REVIEW,
                               "Review typed message",
                               NULL,
                               g_finishMsg,
                               NULL,
                               &warning,
                               choiceCallback);
}

/**
 * @brief Start EIP712 signature review flow
 *
 * @param filtering_mode the filtering mode to use
 * @return status code indicating success or failure
 */
uint16_t ui_sign_712(e_eip712_filtering_mode filtering_mode) {
    // Initialize the pairs list
    ui_712_push_pairs();

    ui_712_start_review(filtering_mode, TYPE_MESSAGE, ui_typed_message_review_choice);
    return SWO_SUCCESS;
}

/**
 * @brief Start EIP712 signature review flow in Legacy (v0) mode
 * @return status code indicating success or failure
 */
uint16_t ui_sign_712_v0(void) {
    uint16_t sw = SWO_PARAMETER_ERROR_NO_INFO;

    sw = ui_712_start(EIP712_FILTERING_BASIC);
    if (sw != SWO_SUCCESS) {
        return sw;
    }

    // Initialize the buffers
    if (!ui_pairs_init(2)) {
        // Initialization failed, cleanup and return
        return SWO_INSUFFICIENT_MEMORY;
    }

    // Initialize the tag/value pairs
    eip712_format_hash(0);

    ui_712_start_review(EIP712_FILTERING_BASIC,
                        TYPE_TRANSACTION,
                        ui_typed_message_review_choice_v0);
    return SWO_SUCCESS;
}
