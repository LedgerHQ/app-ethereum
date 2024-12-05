#ifdef HAVE_NBGL

#include "os_pic.h"
#include "common_ui.h"
#include "glyphs.h"
#include "ui_nbgl.h"
#include "nbgl_use_case.h"
#include "nbgl_layout.h"
#include "nbgl_obj.h"
#include "os_io_seproxyhal.h"
#include "cmd_getTxSimulation.h"
#include "network_icons.h"
#include "feature_signTx.h"
#include "apdu_constants.h"

static void tx_simulation_cb(int token, uint8_t index);

#ifdef TARGET_STAX
#define IMPORTANT_ICON C_Important_Circle_32px
#define REPORT_ICON    C_w3c_report_32px
#elif defined(TARGET_FLEX)
#define IMPORTANT_ICON C_Important_Circle_64px
#define REPORT_ICON    C_w3c_report_40px
#endif  // TARGET_STAX

// contexts for background and modal pages
static nbgl_layout_t layoutCtx = {0};
// global flag to know if we are in demo mode
static bool g_is_demo = false;
// global flag to know if we are called from plugin
static bool g_from_plugin = false;

// clang-format off
enum {
    TOKEN_BACK = FIRST_USER_TOKEN,
    TOKEN_INFO,
    TOKEN_CHOICE,
    TOKEN_BS,
    TOKEN_RISK
};
// clang-format on

static void ui_display_tx_cb(bool confirm) {
    nbgl_reviewStatusType_t status =
        confirm ? STATUS_TYPE_TRANSACTION_SIGNED : STATUS_TYPE_TRANSACTION_REJECTED;
    nbgl_useCaseReviewStatus(status, ui_idle);
    clearTxSimulation();
}

/**
 * @brief Display the transaction simulation received values - PoC only
 *
 */
static void ui_display_tx_simulation_values(void) {
    static nbgl_contentTagValue_t pairs[10] = {0};
    static nbgl_contentTagValueList_t pairsList = {0};
    static char chain_str[sizeof(uint64_t) * 2 + 1] = {0};
    static char hash_str[HASH_SIZE * 2 + 1] = {0};
    static char risk_str[20] = {0};
    static char score_str[30] = {0};
    static char category_str[30] = {0};
    int nbPairs = 0;

    // Format the strings of parameters
    u64_to_string(TX_SIMULATION.chain_id, chain_str, sizeof(chain_str));
    bytes_to_hex(hash_str, sizeof(hash_str), TX_SIMULATION.tx_hash, HASH_SIZE);
    snprintf(risk_str, sizeof(risk_str), "%d (0x%x)", TX_SIMULATION.risk, TX_SIMULATION.risk);
    snprintf(score_str, sizeof(score_str), "%d - %s", TX_SIMULATION.score, getTxSimuScoreStr());
    snprintf(category_str,
             sizeof(category_str),
             "%d - %s",
             TX_SIMULATION.category,
             getTxSimuCategoryStr());

    pairs[nbPairs].item = "chain ID";
    pairs[nbPairs++].value = chain_str;
    pairs[nbPairs].item = "TX Hash";
    pairs[nbPairs++].value = hash_str;
    pairs[nbPairs].item = "Risk";
    pairs[nbPairs++].value = risk_str;
    pairs[nbPairs].item = "Score";
    pairs[nbPairs++].value = score_str;
    pairs[nbPairs].item = "Category";
    pairs[nbPairs++].value = category_str;
    pairs[nbPairs].item = "Provider msg";
    pairs[nbPairs++].value = TX_SIMULATION.provider_msg;
    pairs[nbPairs].item = "Tiny URL";
    pairs[nbPairs++].value = TX_SIMULATION.tiny_url;
    pairsList.nbPairs = nbPairs;
    pairsList.pairs = pairs;

    nbgl_useCaseReviewLight(TYPE_TRANSACTION,
                            &pairsList,
                            get_network_icon_from_chain_id(&TX_SIMULATION.chain_id),
                            "TX Simulation",
                            NULL,
                            "Continue",
                            ui_display_tx_cb);
}

/**
 * @brief Display the transaction simulation details
 *
 * @param[in] token Use case to display
 */
static void ui_display_tx_simulation_details(uint8_t token) {
    int status = -1;
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_layoutHeader_t header = {0};
    nbgl_layoutQRCode_t qrCode = {0};
    nbgl_contentCenter_t contentCenter = {0};
    static char subText[PARTNER_SIZE + MSG_SIZE + 10] = {0};

    // Add page layout
    layoutDescription.onActionCallback = tx_simulation_cb;
    layoutCtx = nbgl_layoutGet(&layoutDescription);

    // Add top bar
    header.type = HEADER_BACK_AND_TEXT;
    if ((TX_SIMULATION.score != SCORE_UNKNOWN) || (token != TOKEN_RISK)) {
        header.separationLine = true;
    }
    if (token == TOKEN_BS) {
        if (TX_SIMULATION.score == SCORE_BENIGN) {
            header.backAndText.token = TOKEN_BACK;
        } else {
            header.backAndText.token = TOKEN_INFO;
        }
        header.backAndText.text = "Blind signing report";
    } else {
        if (tmpContent.txContent.dataPresent) {
            header.backAndText.token = TOKEN_INFO;
        } else {
            header.backAndText.token = TOKEN_BACK;
        }
        if (TX_SIMULATION.score == SCORE_UNKNOWN) {
            header.backAndText.text = "Web3 Checks report";
        } else {
            snprintf(subText, sizeof(subText), "%s report", TX_SIMULATION.partner);
            header.backAndText.text = subText;
        }
    }
    header.backAndText.tuneId = TUNE_TAP_CASUAL;
    status = nbgl_layoutAddHeader(layoutCtx, &header);
    if (status < 0) {
        return;
    }

    if ((token == TOKEN_RISK) && (TX_SIMULATION.score == SCORE_UNKNOWN)) {
        // W3C Issue, Add Center Info (icon and info)
        contentCenter.icon = &IMPORTANT_ICON;
        contentCenter.title = "Web3 Checks could not verify this transaction";
        contentCenter.description =
            "An issue prevented Web3 Checks from running. Get help: ledger.com/e11";
        status = nbgl_layoutAddContentCenter(layoutCtx, &contentCenter);
        if (status < 0) {
            return;
        }
    } else {
        // Add QrCode
        if (token == TOKEN_BS) {
            qrCode.url = "ledger.com/e8";
            qrCode.text1 = "ledger.com/e8";
            header.backAndText.text = "Blind signing report";
            qrCode.text2 = "Scan to learn about the risks of blind signing.";
        } else {
            qrCode.url = PIC(TX_SIMULATION.tiny_url);
            qrCode.text1 = PIC(TX_SIMULATION.tiny_url);
            qrCode.text2 = "Scan to view the full report.";
        }
        qrCode.largeText1 = true;
        qrCode.offsetY = 40;
        status = nbgl_layoutAddQRCode(layoutCtx, &qrCode);
        if (status < 0) {
            return;
        }
    }

    // Draw the page
    nbgl_layoutDraw(layoutCtx);
}

/**
 * @brief Display the transaction simulation security report
 *
 */
static void ui_display_tx_simulation_report_security(void) {
    int status = -1;
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_layoutHeader_t header = {0};
    nbgl_layoutBar_t bar = {0};
    static char subText[PARTNER_SIZE + MSG_SIZE + 20] = {0};

    // Add page layout
    layoutDescription.onActionCallback = tx_simulation_cb;
    layoutCtx = nbgl_layoutGet(&layoutDescription);

    // Add top bar
    header.type = HEADER_BACK_AND_TEXT;
    header.separationLine = true;
    header.backAndText.text = "Security report";
    header.backAndText.token = TOKEN_BACK;
    header.backAndText.tuneId = TUNE_TAP_CASUAL;
    status = nbgl_layoutAddHeader(layoutCtx, &header);
    if (status < 0) {
        return;
    }

    // Add touchable bar for the Blind Signing
    bar.text = "Blind signing";
    bar.subText = "This transaction cannot be fully decoded.";
    bar.iconLeft = &WARNING_ICON;
    bar.iconRight = &CHEVRON_NEXT_ICON;
    bar.token = TOKEN_BS;
    bar.tuneId = TUNE_TAP_CASUAL;
    status = nbgl_layoutAddTouchableBar(layoutCtx, &bar);
    if (status < 0) {
        return;
    }
    // Add touchable bar for the Risk
    switch (TX_SIMULATION.score) {
        case SCORE_UNKNOWN:
            bar.text = "Web3 Checks issue";
            bar.subText = "Web3 Checks could not verify this transaction.";
            break;
        case SCORE_WARNING:
            bar.text = "Risk detected";
            snprintf(subText,
                     sizeof(subText),
                     "%s found a risk: %s",
                     TX_SIMULATION.partner,
                     TX_SIMULATION.provider_msg);
            bar.subText = subText;
            break;
        case SCORE_MALICIOUS:
            bar.text = "Threat detected";
            snprintf(subText,
                     sizeof(subText),
                     "%s found a threat: %s",
                     TX_SIMULATION.partner,
                     TX_SIMULATION.provider_msg);
            bar.subText = subText;
            break;
        default:
            // Should not happen!
            break;
    }
    bar.iconLeft = &IMPORTANT_ICON;
    bar.iconRight = &CHEVRON_NEXT_ICON;
    bar.token = TOKEN_RISK;
    bar.tuneId = TUNE_TAP_CASUAL;
    status = nbgl_layoutAddTouchableBar(layoutCtx, &bar);
    if (status < 0) {
        return;
    }

    // Draw the page
    nbgl_layoutDraw(layoutCtx);
}

/**
 * @brief Callback called when navigating on 'TX Simulation' result page
 *
 * @param token Integer identifying the touched control widget
 * @param index Value of the activated widget (for radio buttons, switches...)
 */
static void tx_simulation_cb(int token, uint8_t index) {
    switch (token) {
        case TOKEN_BS:
            // Detail 'Blind Signing' bar button
            ui_display_tx_simulation_details(TOKEN_BS);
            break;
        case TOKEN_RISK:
            // Detail 'Risk' bar button
            ui_display_tx_simulation_details(TOKEN_RISK);
            break;
        case TOKEN_INFO:
            // Top right button
            if (tmpContent.txContent.dataPresent) {
                if (TX_SIMULATION.score == SCORE_BENIGN) {
                    // Blind Signing only Info
                    ui_display_tx_simulation_details(TOKEN_BS);
                } else {
                    // Blind Signing + Risk Info
                    ui_display_tx_simulation_report_security();
                }
            } else {
                // Risk only Info
                ui_display_tx_simulation_details(TOKEN_RISK);
            }
            break;
        case TOKEN_CHOICE:
            if ((TX_SIMULATION.score == SCORE_BENIGN) || (index == 1)) {
                if (g_is_demo) {
                    // Display the parameters without starting the TX
                    ui_display_tx_simulation_values();
                } else {
                    // Start TX
                    ux_approve_tx(g_from_plugin);
                }
                break;
            } else {
                // Back to safety
                if (!g_is_demo) {
                    // In case of demo, the
                    io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED,
                                              0,
                                              true,
                                              false);
                }
                clearTxSimulation();
                nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_idle);
            }
            break;
        case TOKEN_BACK:
            // Quit the page
            ui_display_tx_simulation(g_from_plugin, g_is_demo);
            break;
        default:
            break;
    }
}

/**
 * @brief Display the transaction simulation result
 *
 * @param[in] fromPlugin Flag indicating if the call is from a plugin
 * @param[in] is_demo Flag to display the parameters without starting the TX
 */
void ui_display_tx_simulation(bool fromPlugin, bool is_demo) {
    int status = -1;
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_contentCenter_t contentCenter = {0};
    nbgl_layoutChoiceButtons_t choiceButtons = {0};
    nbgl_layoutButton_t button = {0};
    static char subText[PARTNER_SIZE + 70] = {0};

    // Add page layout
    layoutDescription.onActionCallback = tx_simulation_cb;
    layoutCtx = nbgl_layoutGet(&layoutDescription);

    if (TX_SIMULATION.score != SCORE_BENIGN || tmpContent.txContent.dataPresent) {
        // Add top right button
        status = nbgl_layoutAddTopRightButton(layoutCtx, &REPORT_ICON, TOKEN_INFO, TUNE_TAP_CASUAL);
        if (status < 0) {
            return;
        }
    }

    // Add Center Info (icon and info)
    switch (TX_SIMULATION.score) {
        case SCORE_UNKNOWN:
            // W3C Down
            if (tmpContent.txContent.dataPresent) {
                // Blind signing detected
                contentCenter.icon = &C_Warning_64px;
                contentCenter.title = "Dangerous transaction:";
                contentCenter.description =
                    "This transaction cannot be fully decoded and was not verified by Web3 Checks.";
            } else {
                contentCenter.icon = &IMPORTANT_ICON;
                contentCenter.title = "Web3 Checks could not verify this message";
                contentCenter.description =
                    "An issue prevented Web3 Checks from running. Get help: ledger.com/e11";
            }
            break;

        case SCORE_BENIGN:
            // W3C Pass, Demo mode only, else, we don't run this function
            contentCenter.icon = &C_Check_Circle_64px;
            contentCenter.title = "NO Threat detected";
            snprintf(subText,
                     sizeof(subText),
                     "This transaction was scanned as benign by %s",
                     TX_SIMULATION.partner);
            contentCenter.description = subText;
            break;

        default:
            contentCenter.icon = &C_Warning_64px;
            if (tmpContent.txContent.dataPresent) {
                // Case Blind Signing + Web3Cheks Malicious
                contentCenter.title = "Dangerous transaction:";
                snprintf(subText,
                         sizeof(subText),
                         "This transaction cannot be fully decoded and was marked as risky by %s",
                         TX_SIMULATION.partner);
            } else {
                if (TX_SIMULATION.score == SCORE_MALICIOUS) {
                    // Case Web3Cheks Malicious
                    contentCenter.title = "Threat detected:";
                    snprintf(subText,
                             sizeof(subText),
                             "This transaction was scanned as malicious by %s",
                             TX_SIMULATION.partner);
                } else {
                    // Case Web3Cheks Warning
                    contentCenter.title = "Risk detected:";
                    snprintf(subText,
                             sizeof(subText),
                             "This transaction was scanned as risky by %s",
                             TX_SIMULATION.partner);
                }
                contentCenter.smallTitle = PIC(TX_SIMULATION.provider_msg);
            }
            contentCenter.description = subText;
            break;
    }
    status = nbgl_layoutAddContentCenter(layoutCtx, &contentCenter);
    if (status < 0) {
        return;
    }

    if (TX_SIMULATION.score == SCORE_BENIGN) {
        // Add button, Demo mode only, else, we don't run this function
        button.text = "Continue";
        button.token = TOKEN_CHOICE;
        button.style = BLACK_BACKGROUND;
        button.tuneId = TUNE_TAP_CASUAL;
        button.onBottom = true;
        status = nbgl_layoutAddButton(layoutCtx, &button);
    } else {
        // Add choice buttons
        choiceButtons.topText = "Back to safety";
        choiceButtons.bottomText = "Continue anyway";
        choiceButtons.token = TOKEN_CHOICE;
        choiceButtons.style = ROUNDED_AND_FOOTER_STYLE;
        choiceButtons.tuneId = TUNE_TAP_CASUAL;
        status = nbgl_layoutAddChoiceButtons(layoutCtx, &choiceButtons);
    }
    if (status < 0) {
        return;
    }

    g_is_demo = is_demo;
    g_from_plugin = fromPlugin;
    // Draw the page
    nbgl_layoutDraw(layoutCtx);
}

#endif  // HAVE_NBGL
