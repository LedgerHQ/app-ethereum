#include "apdu_constants.h"
#include "utils.h"
#include "nbgl_use_case.h"
#include "glyphs.h"
#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "ui_callbacks.h"
#include "cmd_get_tx_simulation.h"

#ifdef HAVE_TRANSACTION_CHECKS

#define HOW_TO_INFO_NB 3

enum {
    WARNING_CHOICE_TOKEN = FIRST_USER_TOKEN,
    WARNING_BUTTON_TOKEN,
    DISMISS_WARNING_TOKEN,
};

// Global flag to send Opt-In result
static bool g_response_expected;

/**
 * @brief Display the transaction simulation error popup
 *
 * @param[in] callback Callback to be called after the user has made a choice
 */
void ui_tx_simulation_error(nbgl_choiceCallback_t callback) {
#ifdef HAVE_PIEZO_SOUND
    io_seproxyhal_play_tune(TUNE_NEUTRAL);
#endif  // HAVE_PIEZO_SOUND
    nbgl_useCaseChoice(&C_Denied_Circle_64px,
                       "Transaction Check failed because of technical reasons",
                       "Reject this transaction and try again. "
                       "If the issue persists, get help at: ledger.com/e9",
                       "Reject transaction",
                       "Continue anyway",
                       callback);
}

/**
 * @brief Callback called when user press a button on the opt-in explain screen
 *
 * @param[in] token Id of the pressed widget (button)
 * @param[in] index Indicates the value of the pressed button
 */
static void opt_in_explain_cb(int token, uint8_t index) {
    UNUSED(token);
    UNUSED(index);
    ui_tx_simulation_opt_in(g_response_expected);
}

/**
 * @brief Display the "How it works" screen
 *
 */
static void ui_tx_simulation_explain(void) {
    nbgl_layout_t *layoutCtx = NULL;
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_layoutLeftContent_t info = {0};
    nbgl_layoutHeader_t headerDesc = {.type = HEADER_BACK_AND_TEXT,
                                      .backAndText.tuneId = TUNE_TAP_CASUAL,
                                      .backAndText.token = DISMISS_WARNING_TOKEN};

    layoutDescription.withLeftBorder = true;
    layoutDescription.onActionCallback = opt_in_explain_cb;
    layoutCtx = nbgl_layoutGet(&layoutDescription);
    const char *rowTexts[HOW_TO_INFO_NB] = {
        "Transaction is checked for threats before signing.",
        "The result is displayed: Critical threat, potential risk or no threat.",
        "Scan the QR Code on your Ledger device for details on any threat or risk.",
    };
    const nbgl_icon_details_t *rowIcons[HOW_TO_INFO_NB] = {
        &PRIVACY_ICON,
        &WARNING_ICON,
        &QRCODE_ICON,
    };

    // add header
    nbgl_layoutAddHeader(layoutCtx, &headerDesc);

    // add title
    info.title = "How it works";
    info.nbRows = HOW_TO_INFO_NB;
    info.rowTexts = rowTexts;
    info.rowIcons = rowIcons;
    nbgl_layoutAddLeftContent(layoutCtx, &info);

    nbgl_layoutDraw(layoutCtx);
    nbgl_refresh();
}

/**
 * @brief Finalise the opt-in process with status
 *
 * @param[in] confirm Flag indicates if the user validates the setting
 * @param[in] callback Callback to display the next screen
 */
static void finalise_opt_in(bool confirm, nbgl_callback_t callback) {
    if (confirm) {
        nbgl_useCaseStatus("Transaction Check enabled", true, callback);
    } else {
        callback();
    }
}

/**
 * @brief Callback called when user press a button on the opt-in screen
 *
 * @param[in] token Id of the pressed widget (button)
 * @param[in] index Indicates the value of the pressed button
 */
static void opt_in_action_cb(int token, uint8_t index) {
    bool opt_in = true;
    bool confirm = true;

    switch (token) {
        case WARNING_CHOICE_TOKEN:
            // Set the opt-in flag
            nvm_write((void *) &N_storage.tx_check_opt_in, (void *) &opt_in, sizeof(opt_in));
            confirm = (index == 0 ? true : false);
            if (confirm != N_storage.tx_check_enable) {
                // Set the Transaction Check flag
                nvm_write((void *) &N_storage.tx_check_enable, (void *) &confirm, sizeof(confirm));
            }
            if (g_response_expected) {
                // just respond the current state and return to idle screen
                G_io_apdu_buffer[0] = N_storage.tx_check_enable;
                io_seproxyhal_send_status(APDU_RESPONSE_OK, 1, false, false);
                finalise_opt_in(confirm, ui_idle);
            } else {
                finalise_opt_in(confirm, ui_settings);
            }
            break;
        case WARNING_BUTTON_TOKEN:
            ui_tx_simulation_explain();
            break;
        default:
            break;
    }
}

/**
 * @brief Display the opt-in screen
 *
 * @param[in] response_expected indicates if a RAPDU is expected
 */
void ui_tx_simulation_opt_in(bool response_expected) {
    nbgl_layout_t *layoutCtx = NULL;
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_contentCenter_t info = {0};
    nbgl_layoutChoiceButtons_t buttonsInfo = {.topText = "Yes, enable",
                                              .bottomText = "Maybe later",
                                              .token = WARNING_CHOICE_TOKEN,
                                              .style = ROUNDED_AND_FOOTER_STYLE,
                                              .tuneId = TUNE_TAP_CASUAL};

    nbgl_layoutHeader_t headerDesc = {.type = HEADER_EXTENDED_BACK,
                                      .separationLine = false,
                                      .extendedBack.actionIcon = &QUESTION_CIRCLE_ICON,
                                      .extendedBack.backToken = NBGL_INVALID_TOKEN,
                                      .extendedBack.tuneId = TUNE_TAP_CASUAL,
                                      .extendedBack.text = NULL,
                                      .extendedBack.actionToken = WARNING_BUTTON_TOKEN};

    g_response_expected = response_expected;
    layoutDescription.withLeftBorder = true;
    layoutDescription.onActionCallback = opt_in_action_cb;
    layoutCtx = nbgl_layoutGet(&layoutDescription);

    // add header
    nbgl_layoutAddHeader(layoutCtx, &headerDesc);

    // add main content
    info.title = "Enable\nTransaction Check?";
    info.description =
        "Get real-time warnings about risky Ethereum transactions. "
        "Powered by service providers.";
    info.subText = "By enabling, you accept T&Cs: ledger.com/tx-check";
    nbgl_layoutAddContentCenter(layoutCtx, &info);

    // add button and footer on bottom
    nbgl_layoutAddChoiceButtons(layoutCtx, &buttonsInfo);

#ifdef HAVE_PIEZO_SOUND
    io_seproxyhal_play_tune(TUNE_LOOK_AT_ME);
#endif  // HAVE_PIEZO_SOUND
    nbgl_layoutDraw(layoutCtx);
    nbgl_refresh();
}

#endif  // HAVE_TRANSACTION_CHECKS

/**
 * @brief Determine the Review finish title prefix.
 *
 * @return Finish title prefix string
 */
const char *ui_tx_simulation_finish_str(void) {
    if (warning.predefinedSet & SET_BIT(W3C_THREAT_DETECTED_WARN)) {
        return "Accept threat and sign";
    }
    if ((warning.predefinedSet & SET_BIT(W3C_RISK_DETECTED_WARN)) ||
        (warning.predefinedSet & SET_BIT(BLIND_SIGNING_WARN))) {
        return "Accept risk and sign";
    }
    return "Sign";
}
