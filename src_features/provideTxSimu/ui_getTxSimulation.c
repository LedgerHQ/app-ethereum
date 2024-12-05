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

static void tx_simulation_cb(int token, uint8_t index);

#ifdef TARGET_STAX
#define IMPORTANT_ICON C_Important_Circle_32px
#elif defined(TARGET_FLEX)
#define IMPORTANT_ICON C_Important_Circle_64px
#endif  // TARGET_STAX

// contexts for background and modal pages
static nbgl_layout_t layoutCtx = {0};

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
    snprintf(score_str, sizeof(score_str), "%d - %s", getTxSimuRiskScore(), getTxSimuRiskScorStr());
    snprintf(category_str,
             sizeof(category_str),
             "%d - %s",
             TX_SIMULATION.category,
             getTxSimuRiskcategory());

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

    // Add page layout
    layoutDescription.onActionCallback = tx_simulation_cb;
    layoutCtx = nbgl_layoutGet(&layoutDescription);

    // Add top bar
    header.type = HEADER_BACK_AND_TEXT;
    header.separationLine = true;
    if (token == TOKEN_BS) {
        header.backAndText.token = TOKEN_INFO;
        header.backAndText.text = "Blind signing report";
    } else {
        if (N_storage.dataAllowed) {
            header.backAndText.token = TOKEN_INFO;
        } else {
            header.backAndText.token = TOKEN_BACK;
        }
        if (getTxSimuRiskScore() == RISK_MALICIOUS) {
            header.backAndText.text = "Web3 Checks\nthreat report";
        } else {
            header.backAndText.text = "Web3 Checks\nrisk report";
        }
    }
    header.backAndText.tuneId = TUNE_TAP_CASUAL;
    status = nbgl_layoutAddHeader(layoutCtx, &header);
    if (status < 0) {
        return;
    }

    // Add separation line
    status = nbgl_layoutAddSeparationLine(layoutCtx);
    if (status < 0) {
        return;
    }

    // Add QrCode
    if (token == TOKEN_BS) {
        qrCode.url = "ledger.com/e8";
        qrCode.text1 = "ledger.com/e8";
        header.backAndText.text = "Blind signing report";
        qrCode.text2 = "Scan to learn about the risks of blind signing.";
    } else {
        qrCode.url = PIC(TX_SIMULATION.tiny_url);
        qrCode.text1 = PIC(TX_SIMULATION.tiny_url);
        if (getTxSimuRiskScore() == RISK_MALICIOUS) {
            qrCode.text2 = "Scan to view the threat report from Blockaid.";
        } else {
            qrCode.text2 = "Scan to view the risk report from Blockaid.";
        }
    }
    qrCode.largeText1 = true;
    qrCode.offsetY = 40;
    status = nbgl_layoutAddQRCode(layoutCtx, &qrCode);
    if (status < 0) {
        return;
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
    tx_simulation_risk_t score = getTxSimuRiskScore();

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

    // Add separation line
    status = nbgl_layoutAddSeparationLine(layoutCtx);
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
    if (score == RISK_WARNING) {
        bar.text = "Risk detected";
        bar.subText = "Web3 Checks found a risk: Losing swap";
    } else if (score == RISK_MALICIOUS) {
        bar.text = "Threat detected";
        bar.subText = "Web3 Checks found a threat: Known drainer contract";
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
    PRINTF("Account callback: %d - %d\n", token, index);
    switch (token) {
        case TOKEN_BS:
            // Top right 'Info' button
            ui_display_tx_simulation_details(TOKEN_BS);
            break;
        case TOKEN_RISK:
            ui_display_tx_simulation_details(TOKEN_RISK);
            break;
        case TOKEN_INFO:
            // Top right 'Info' button
            if (N_storage.dataAllowed) {
                ui_display_tx_simulation_report_security();
            } else {
                ui_display_tx_simulation_details(TOKEN_RISK);
            }
            break;
        case TOKEN_CHOICE:
            if ((getTxSimuRiskScore() == RISK_BENIGN) || (index == 1)) {
                // Start TX
                ui_display_tx_simulation_values();
                break;
            } else {
                // Back to safety
                nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_idle);
            }
            break;
        case TOKEN_BACK:
            // Quit the page
            ui_display_tx_simulation();
            break;
        default:
            break;
    }
}

/**
 * @brief Display the transaction simulation result
 *
 */
void ui_display_tx_simulation(void) {
    int status = -1;
    nbgl_layoutDescription_t layoutDescription = {0};
    nbgl_contentCenter_t contentCenter = {0};
    nbgl_layoutChoiceButtons_t choiceButtons = {0};
    nbgl_layoutButton_t button = {0};
    tx_simulation_risk_t score = getTxSimuRiskScore();

    // Add page layout
    layoutDescription.onActionCallback = tx_simulation_cb;
    layoutCtx = nbgl_layoutGet(&layoutDescription);

    if (score != RISK_BENIGN) {
        // Add top right button
        status = nbgl_layoutAddTopRightButton(layoutCtx,
                                              &C_SecurityShield_64px,
                                              TOKEN_INFO,
                                              TUNE_TAP_CASUAL);
        if (status < 0) {
            return;
        }
    }

    // Add Center Info (icon and info)
    contentCenter.icon = &C_Warning_64px;
    if (score == RISK_BENIGN) {
        contentCenter.icon = &C_Check_Circle_64px;
        contentCenter.title = "NO Threat detected";
        contentCenter.description = "This transaction was scanned as benign by Web3 Checks.";
    } else {
        if (N_storage.dataAllowed) {
            // Case Blind Signing + Web3Cheks Malicious
            contentCenter.title = "Dangerous transaction:";
            contentCenter.description =
                "This transaction cannot be fully decoded and was marked as risky by Web3 Checks.";
        } else {
            if (score == RISK_MALICIOUS) {
                // Case Web3Cheks Malicious
                contentCenter.title = "Threat detected:";
                contentCenter.smallTitle = "Known drainer contract";
                contentCenter.description =
                    "This transaction was scanned as malicious by Web3 Checks.";
            } else {
                // Case Web3Cheks Warning
                contentCenter.title = "Risk detected:";
                contentCenter.smallTitle = "Losing trade";
                contentCenter.description = "This transaction was scanned as risky by Web3 Checks.";
            }
        }
    }
    status = nbgl_layoutAddContentCenter(layoutCtx, &contentCenter);
    if (status < 0) {
        return;
    }

    if (score == RISK_BENIGN) {
        // Add button
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

    // Draw the page
    nbgl_layoutDraw(layoutCtx);
}

#endif  // HAVE_NBGL
