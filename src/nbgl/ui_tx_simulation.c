#include "apdu_constants.h"
#include "utils.h"
#include "nbgl_use_case.h"
#include "ui_nbgl.h"
#include "common_ui.h"
#include "cmd_get_tx_simulation.h"

#ifdef HAVE_TRANSACTION_CHECKS

// Global flag to send Opt-In result
static bool g_response_expected;

// "How it works" page, displayed from the opt-in screen
static nbgl_genericDetails_t g_how_it_works;

#define HOW_IT_WORKS_NB 3

static const char *g_how_it_works_texts[HOW_IT_WORKS_NB];
static const nbgl_icon_details_t *g_how_it_works_icons[HOW_IT_WORKS_NB];

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
 * @brief Callback called when user makes a choice on the opt-in screen
 *
 * @param[in] confirm Indicates if the user enables the setting
 */
static void opt_in_choice_cb(bool confirm) {
    bool opt_in = true;

    // Set the opt-in flag
    nvm_write((void *) &N_storage.tx_check_opt_in, (void *) &opt_in, sizeof(opt_in));
    if (confirm != N_storage.tx_check_enable) {
        // Set the Transaction Check flag
        nvm_write((void *) &N_storage.tx_check_enable, (void *) &confirm, sizeof(confirm));
    }
    if (g_response_expected) {
        // just respond the current state and return to idle screen
        G_io_tx_buffer[0] = N_storage.tx_check_enable;
        io_seproxyhal_send_status(SWO_SUCCESS, 1, false, false);
        finalise_opt_in(confirm, ui_idle);
    } else {
        finalise_opt_in(confirm, ui_settings);
    }
}

/**
 * @brief Display the opt-in screen
 *
 * @param[in] response_expected indicates if a RAPDU is expected
 */
void ui_tx_simulation_opt_in(bool response_expected) {
    g_response_expected = response_expected;

    g_how_it_works_texts[0] = "Transaction is checked for threats before signing.";
    g_how_it_works_texts[1] =
        "The result is displayed: Critical threat, potential risk or no threat.";
    g_how_it_works_texts[2] =
        "Scan the QR Code on your Ledger device for details on any threat or risk.";
    g_how_it_works_icons[0] = &PRIVACY_ICON;
    g_how_it_works_icons[1] = &WARNING_ICON;
    g_how_it_works_icons[2] = &QRCODE_ICON;

    explicit_bzero(&g_how_it_works, sizeof(g_how_it_works));
    g_how_it_works.type = LEFT_CONTENT_WARNING;
    g_how_it_works.leftContent.title = "How it works";
    g_how_it_works.leftContent.nbRows = HOW_IT_WORKS_NB;
    g_how_it_works.leftContent.rowTexts = g_how_it_works_texts;
    g_how_it_works.leftContent.rowIcons = g_how_it_works_icons;

    nbgl_useCaseAdvancedChoiceWithDetails(NULL,
                                          &QUESTION_CIRCLE_ICON,
                                          "Enable\nTransaction Check?",
                                          "Real-time warnings powered\nby 3rd service providers.",
                                          "By enabling, you accept T&Cs: ledger.com/tx-check",
                                          "Yes, enable",
                                          "Maybe later",
                                          &g_how_it_works,
                                          opt_in_choice_cb);
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
