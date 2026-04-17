/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file handle_ledger_account.c
 * @brief Coin-app callbacks for the Register and Edit Ledger Account flows
 *
 * Implements the coin-app callbacks required by the SDK:
 *
 * Register Ledger Account:
 *  - handle_check_ledger_account(): validate the parsed account data
 *  - display_register_ledger_account_review(): display the Ledger Account registration review
 * screen
 *  - finalize_ui_register_ledger_account(): clean up after user decision
 *
 * Edit Ledger Account:
 *  - finalize_ui_edit_ledger_account(): clean up after user decision
 */

#include "ledger_account.h"
#include "chain_config.h"
#include "apdu_constants.h"
#include "common_utils.h"
#include "network.h"
#include "ui_nbgl.h"
#include "ui_callbacks.h"
#include "ui_utils.h"
#include "app_mem_utils.h"
#include "get_public_key.h"
#include "io.h"
#include "ox_ec.h"
#include "network_icons.h"

#if defined(HAVE_ADDRESS_BOOK) && defined(HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT)

/* Private defines -----------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/**
 * @brief Context for Register Ledger Account flow
 */
typedef struct {
    char *address_display;
    char *network_display;
} register_ledger_account_ctx_t;

/**
 * @brief Global context for all Ledger Account-related UI flows
 */
typedef struct {
    ledger_account_t *ledger_account;
    register_ledger_account_ctx_t register_account;
} ledger_account_ui_ctx_t;

/* Private variables ---------------------------------------------------------*/
static ledger_account_ui_ctx_t g_ctx = {0};

/* Exported functions --------------------------------------------------------*/

/* =========================================================================
 * Register Ledger Account callbacks
 * =========================================================================
 */

/**
 * @brief Handle called to finalize the UI flow for registering a Ledger Account
 */
void finalize_ui_register_ledger_account(void) {
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_account.address_display);
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_account.network_display);
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.ledger_account);
    ui_idle();
}

/**
 * @brief Handle called to validate the received Ledger Account
 *
 * Validates derivation path length and chain ID range for Ethereum.
 *
 * @param[in] params Structure containing the ledger account to validate
 * @return true if the account is valid, false to reject
 */
bool handle_check_ledger_account(ledger_account_t *params) {
    cx_ecfp_public_key_t publicKey = {0};
    PRINTF("Inside handle_check_ledger_account\n");
    if (params == NULL) {
        PRINTF("params is NULL\n");
        return false;
    }
    // Check if the blockchain family is supported
    if (params->blockchain_family != FAMILY_ETHEREUM) {
        PRINTF("Unsupported blockchain family: %d\n", params->blockchain_family);
        return false;
    }
    if (params->bip32_path.length == 0 || params->bip32_path.length > MAX_BIP32_PATH) {
        PRINTF("Invalid derivation path length: %d\n", params->bip32_path.length);
        return false;
    }
    if ((params->chain_id > MAX_VALID_CHAIN_ID) || (params->chain_id == 0)) {
        PRINTF("Unsupported chain ID: %llu\n", params->chain_id);
        return false;
    }

    g_ctx.register_account.address_display = APP_MEM_ALLOC(ADDRESS_LENGTH_HEX_STR);
    if (g_ctx.register_account.address_display == NULL) {
        PRINTF("Failed to allocate address display buffer\n");
        return false;
    }
    g_ctx.register_account.address_display[0] = '0';
    g_ctx.register_account.address_display[1] = 'x';
    if (get_public_key_string((bip32_path_t *) &params->bip32_path,
                              publicKey.W,
                              g_ctx.register_account.address_display + 2,
                              NULL,
                              params->chain_id) != CX_OK) {
        PRINTF("Failed to get public key\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_account.address_display);
        return false;
    }
    PRINTF("Ledger account validation successful\n");

    g_ctx.ledger_account = APP_MEM_ALLOC(sizeof(ledger_account_t));
    if (g_ctx.ledger_account == NULL) {
        PRINTF("Failed to allocate ledger_account\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_account.address_display);
        return false;
    }
    memmove(g_ctx.ledger_account, params, sizeof(ledger_account_t));
    return true;
}

/**
 * @brief Handle called to display the Ledger Account review UI
 */
void display_register_ledger_account_review(nbgl_choiceCallback_t choice_callback) {
    static const char *texts[1] = {NULL};
    static const char *subTexts[1] = {NULL};
    static nbgl_warningDetails_t details = {0};
    const nbgl_icon_details_t *icon = NULL;

    details.title = g_ctx.ledger_account->account_name;
    details.type = BAR_LIST_WARNING;
    details.barList.nbBars = 1;
    details.barList.texts = texts;
    details.barList.subTexts = subTexts;
    texts[0] = "Address";
    subTexts[0] = g_ctx.register_account.address_display;

    icon = get_network_icon_from_chain_id(&g_ctx.ledger_account->chain_id);
    nbgl_useCaseChoiceWithDetails(icon,
                                  "Confirm name?",
                                  g_ctx.ledger_account->account_name,
                                  "Confirm",
                                  "Cancel",
                                  &details,
                                  choice_callback);
}

/* =========================================================================
 * Edit Ledger Account callbacks
 * =========================================================================
 */

/**
 * @brief Handle called to finalize the UI flow for editing a Ledger Account.
 */
void finalize_ui_edit_ledger_account(void) {
    ui_idle();
}

#endif  // HAVE_ADDRESS_BOOK && HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
