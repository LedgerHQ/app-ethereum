/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file handle_ledger_account.c
 * @brief Coin-app callbacks for the Register and Edit Ledger Account flows
 *
 * Implements the coin-app callbacks required by the SDK:
 *
 * Register Ledger Account:
 *  - handle_check_register_ledger_account(): validate the parsed account data
 *  - display_register_ledger_account_review(): display the Ledger Account registration review
 * screen
 *
 * Edit Ledger Account (reuses the Register review UI):
 *  - handle_check_edit_ledger_account(): validate + prepare the shared review context
 *
 * finalize_ui_ledger_account(): clean up after user decision (shared by Register and Edit)
 *
 * Provide Ledger Account Contact:
 *  - handle_provide_ledger_account(): derive address, store in shared contact list
 */

#if defined(HAVE_ADDRESS_BOOK) && defined(HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT)

#include <string.h>
#include "address_book_ctx.h"
#include "address_book_entrypoints.h"
#include "chain_config.h"
#include "apdu_constants.h"
#include "common_utils.h"
#include "network.h"
#include "ui_nbgl.h"
#include "common_ui.h"
#include "ui_utils.h"
#include "app_mem_utils.h"
#include "get_public_key.h"
#include "io.h"
#include "ox_ec.h"
#include "ui_icons.h"
#include "crypto_helpers.h"
#include "handle_contacts.h"

/* Private defines -----------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
#define g_ctx g_ab_ctx.ledger_account

/* Private functions ---------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* =========================================================================
 * Register Ledger Account callbacks
 * =========================================================================
 */

/**
 * @brief Handle called to finalize the UI flow for a Ledger Account operation.
 *
 * Shared by the Register and Edit flows (they use the same review UI).
 */
void finalize_ui_ledger_account(void) {
    ui_pairs_cleanup();
    address_book_review_cleanup();
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
bool handle_check_register_ledger_account(ledger_account_t *params) {
    cx_ecfp_public_key_t publicKey = {0};
    PRINTF("Inside handle_check_register_ledger_account\n");
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

    if (APP_MEM_PERMANENT((void **) &g_ctx.register_account.address_display,
                          ADDRESS_LENGTH_HEX_STR) == false) {
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

    if (APP_MEM_PERMANENT((void **) &g_ctx.ledger_account, sizeof(ledger_account_t)) == false) {
        PRINTF("Failed to allocate ledger_account\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_account.address_display);
        return false;
    }
    memmove(g_ctx.ledger_account, params, sizeof(ledger_account_t));
    address_book_review_set_flow(AB_FLOW_LEDGER_ACCOUNT);
    return true;
}

/**
 * @brief Handle called to display the Ledger Account review UI
 */
void display_register_ledger_account_review(nbgl_choiceCallback_t choice_callback) {
    register_ledger_account_ctx_t *ra = &g_ctx.register_account;
    const nbgl_icon_details_t *icon =
        get_network_icon_from_chain_id(&g_ctx.ledger_account->chain_id);
    if (icon == NULL) {
        icon = get_app_icon(false);
    }

    // Full address kept for the details modal (accessible via detail header icon)
    ra->texts[0] = "Address";
    ra->subTexts[0] = ra->address_display;
    ra->details.title = g_ctx.ledger_account->account_name;
    ra->details.type = BAR_LIST_WARNING;
    ra->details.barList.nbBars = 1;
    ra->details.barList.texts = ra->texts;
    ra->details.barList.subTexts = ra->subTexts;

#ifdef SCREEN_SIZE_WALLET
    // Middle-truncated address for the subMessage line: "0x1234...5678"
    snprintf(ra->address_display_short,
             sizeof(ra->address_display_short),
             "0x%.*s...%s",
             ADDR_SHORT_HEX_LEN,
             ra->address_display + 2,
             ra->address_display + 2 + ADDRESS_LENGTH_HEX - ADDR_SHORT_HEX_LEN);

    nbgl_useCaseAdvancedChoiceWithDetails(icon,
                                          &INFO_I_ICON,
                                          "Confirm name?",
                                          g_ctx.ledger_account->account_name,
                                          ra->address_display_short,
                                          "Confirm",
                                          "Cancel",
                                          &ra->details,
                                          choice_callback);
#else
    if (ui_pairs_init(2) == false) {
        PRINTF("Failed to initialize pairs\n");
        // The SDK already answered this command with SWO_NO_RESPONSE.
        // Returning without displaying anything would leave the host with no reply at all,
        // so take the rejection path: it sends the error and runs the finalizer.
        choice_callback(false);
        return;
    }
    g_pairs[0].item = "Account name";
    g_pairs[0].value = g_ctx.ledger_account->account_name;
    g_pairs[1].item = "Address";
    g_pairs[1].value = ra->address_display;

    nbgl_useCaseReview(TYPE_OPERATION | ADDRESS_BOOK_OPERATION,
                       g_pairsList,
                       icon,
                       "Review account name",
                       NULL,
                       "Confirm account name",
                       choice_callback);
#endif
}

/* =========================================================================
 * Edit Ledger Account callbacks
 * =========================================================================
 */

/**
 * @brief Handle called to validate the Edit Ledger Account parameters.
 *
 * Called by the SDK after TLV parsing and HMAC verification, before the review
 * UI is displayed. The Edit flow reuses the Register review
 * (display_register_ledger_account_review), so this validates the account and
 * prepares the shared review context exactly like the Register check. It also
 * stores the raw address in @p params->address so that
 * on_edit_ledger_account_applied() can locate the cache entry without
 * re-deriving.
 *
 * @param[in,out] params Parsed edit data; address field is filled on success
 * @return true if validation succeeded, false to reject
 */
bool handle_check_edit_ledger_account(edit_ledger_account_t *params) {
    uint8_t raw_pubkey[CX_SECP256_PUB_KEY_SIZE] = {0};

    PRINTF("Inside handle_check_edit_ledger_account\n");
    if (params == NULL) {
        PRINTF("NULL parameter\n");
        return false;
    }

    // Validate and prepare the shared review context exactly like Register,
    // so the Edit flow can reuse display_register_ledger_account_review().
    if (!handle_check_register_ledger_account(&params->ledger_account)) {
        return false;
    }

    // Edit-specific: keep the raw address so on_edit_ledger_account_applied()
    // can locate the cache entry without re-deriving.
    if (bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                    params->ledger_account.bip32_path.path,
                                    params->ledger_account.bip32_path.length,
                                    raw_pubkey,
                                    NULL,
                                    CX_SHA512) != CX_OK) {
        PRINTF("Key derivation failed\n");
        // handle_check_register_ledger_account() took ownership; release it here since
        // no review will run.
        address_book_review_cleanup();
        return false;
    }
    getEthAddressFromRawKey(raw_pubkey, params->address);
    explicit_bzero(raw_pubkey, sizeof(raw_pubkey));
    params->address_len = ADDRESS_LENGTH;
    return true;
}

/**
 * @brief Notification that a Ledger Account rename was successfully applied.
 *
 * Called by the SDK only when the user confirmed the rename and the new HMAC
 * proof was successfully sent to the host. Uses the address pre-computed by
 * handle_check_edit_ledger_account() to update the cache entry in place.
 *
 * @param[in] edit Parsed edit data (address, chain_id, new account_name)
 */
void on_edit_ledger_account_applied(const edit_ledger_account_t *edit) {
    if (edit == NULL) {
        return;
    }
    update_ledger_account_contact_name(edit->address,
                                       edit->ledger_account.chain_id,
                                       edit->ledger_account.account_name);
}

/**
 * @brief Handle called to store a verified Ledger Account contact.
 *
 * Derives the Ethereum address from the BIP32 path, then allocates and
 * appends a node to the Ledger Account contact list.
 *
 * @param[in] account Verified Ledger Account (account_name, bip32_path,
 *                    chain_id, blockchain_family)
 * @return true if the contact was accepted and stored, false to reject
 */
bool handle_provide_ledger_account(const ledger_account_t *account) {
    uint8_t raw_pubkey[CX_SECP256_PUB_KEY_SIZE] = {0};
    uint8_t address[ADDRESS_LENGTH] = {0};
    s_ab_contact *node = NULL;

    PRINTF("Inside handle_provide_ledger_account\n");
    if (account == NULL) {
        PRINTF("NULL parameter\n");
        return false;
    }
    if (account->blockchain_family != FAMILY_ETHEREUM) {
        PRINTF("Unsupported blockchain family: %d\n", account->blockchain_family);
        return false;
    }
    if ((account->chain_id == 0) || (account->chain_id > MAX_VALID_CHAIN_ID)) {
        PRINTF("Unsupported chain ID: %llu\n", account->chain_id);
        return false;
    }

    // Derive the address first so the duplicate check can run before allocating
    if (bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                    account->bip32_path.path,
                                    account->bip32_path.length,
                                    raw_pubkey,
                                    NULL,
                                    CX_SHA512) != CX_OK) {
        PRINTF("Key derivation failed\n");
        return false;
    }
    getEthAddressFromRawKey(raw_pubkey, address);
    explicit_bzero(raw_pubkey, sizeof(raw_pubkey));

    {
        const s_ab_contact *existing = get_address_book_contact(account->chain_id, address);
        if (existing != NULL && existing->type == AB_CONTACT_LEDGER_ACCOUNT) {
            PRINTF("Contact already stored, ignoring\n");
            return true;
        }
    }

    if (APP_MEM_PERMANENT((void **) &node, sizeof(*node)) == false) {
        PRINTF("Failed to allocate identity\n");
        return false;
    }

    node->type = AB_CONTACT_LEDGER_ACCOUNT;
    strlcpy(node->contact_name, account->account_name, sizeof(node->contact_name));
    memcpy(node->identifier, address, ADDRESS_LENGTH);
    node->chain_id = account->chain_id;
    // scope is empty — Ledger Accounts have no external scope

    ab_contact_list_push(node);

    PRINTF("Stored '%s' (chain %llu)\n", node->contact_name, node->chain_id);
    return true;
}

#endif  // HAVE_ADDRESS_BOOK && HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
