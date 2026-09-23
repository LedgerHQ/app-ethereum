/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file handle_identity.c
 * @brief Coin-app callbacks for Identity flows
 *
 * Register Identity callbacks:
 *  - handle_check_register_identity(): validate the parsed identity data
 *  - get_register_identity_tagValue(): populate the NBGL review screen
 *  - finalize_ui_register_identity(): clean up after user decision
 *
 * Edit Contact Name / Edit Scope Name:
 *  Display is handled entirely by the SDK. The app only provides:
 *  - finalize_ui_edit_contact_name(): clean up after user decision
 *  - finalize_ui_edit_scope_name(): clean up after user decision
 *
 * Edit Identifier callbacks:
 *  - handle_check_edit_identifier(): validate the new identifier and store params
 *  - get_edit_identifier_tagValue(): populate the Edit review screen
 *  - finalize_ui_edit_identifier(): clean up after user decision
 *
 * Provide Identity:
 *  - handle_provide_identity(): store a verified contact in the shared list
 */

#if defined(HAVE_ADDRESS_BOOK)

#include <string.h>
#include "address_book_entrypoints.h"
#include "common_utils.h"
#include "network.h"
#include "ui_nbgl.h"
#include "app_mem_utils.h"
#include "os_utils.h"
#include "chain_config.h"
#include "handle_contacts.h"
#include "address_book_ctx.h"

/* Private defines -----------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
#define g_ctx g_ab_ctx.identity

/* Private functions ---------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/* =========================================================================
 * Register Contact (Identity) callbacks
 * =========================================================================
 */

/**
 * @brief Handle called to finalize the UI flow for registering an Identity
 */
void finalize_ui_register_identity(void) {
    address_book_review_cleanup();
    ui_idle();
}

/**
 * @brief Handle called to validate the received Identity
 *
 * @note The identifier is a 20-byte address.
 *
 * @param[in] params Structure containing the identity to validate
 * @return true if identity is valid, false to reject
 */
bool handle_check_register_identity(identity_t *params) {
    char checksummed_address[ADDRESS_LENGTH_STR] = {0};

    PRINTF("Inside handle_check_register_identity\n");
    if (params == NULL) {
        PRINTF("NULL parameter\n");
        return false;
    }
    // Check if the address length is valid
    if (params->identifier_len != ADDRESS_LENGTH) {
        PRINTF("Invalid address length: %d (expected %d)\n",
               params->identifier_len,
               ADDRESS_LENGTH);
        return false;
    }
    // Check if the blockchain family is supported
    if (params->blockchain_family != FAMILY_ETHEREUM) {
        PRINTF("Unsupported blockchain family: %d\n", params->blockchain_family);
        return false;
    }
    // Check if the chain ID is supported
    if ((params->chain_id > MAX_VALID_CHAIN_ID) || (params->chain_id == 0)) {
        PRINTF("Unsupported chain ID: %llu\n", params->chain_id);
        return false;
    }
    // Generate the checksummed address from the raw bytes
    if (!getEthAddressStringFromBinary((const uint8_t *) params->identifier,
                                       checksummed_address,
                                       params->chain_id)) {
        PRINTF("Failed to generate checksummed address\n");
        return false;
    }
    PRINTF("Address validation successful\n");

    if (APP_MEM_PERMANENT((void **) &g_ctx.register_identity.identity, sizeof(identity_t)) ==
        false) {
        PRINTF("Failed to allocate identity\n");
        return false;
    }
    memmove(g_ctx.register_identity.identity, params, sizeof(identity_t));
    address_book_review_set_flow(AB_FLOW_REGISTER_IDENTITY);
    return true;
}

/**
 * @brief Callback to retrieve a tag-value pair for the Register Identity UI
 *
 * Pair 0: contact name
 * Pair 1: address name (contact scope)
 * Pair 2: identifier (hex with "0x" prefix)
 * Pair 3: network
 *
 * @param[in] pairIndex Index of the tag-value pair
 * @return Pointer to the pair, or NULL if index is invalid
 */
nbgl_contentTagValue_t *get_register_identity_tagValue(uint8_t pairIndex) {
    // The pair lives in a union shared with the other flows: clear every field, not
    // just the two set below, or NBGL reads leftover bytes as valueIcon/extension.
    explicit_bzero(&g_ctx.current_pair, sizeof(g_ctx.current_pair));
    switch (pairIndex) {
        case 0:
            g_ctx.current_pair.item = "Contact name";
            g_ctx.current_pair.value = g_ctx.register_identity.identity->contact_name;
            break;

        case 1:
            g_ctx.current_pair.item = "Address name";
            g_ctx.current_pair.value = g_ctx.register_identity.identity->scope;
            break;

        case 2:
            if (APP_MEM_PERMANENT((void **) &g_ctx.register_identity.identifier_display,
                                  ADDRESS_LENGTH_HEX_STR) == false) {
                PRINTF("Failed to allocate identifier display buffer\n");
                return NULL;
            }
            g_ctx.register_identity.identifier_display[0] = '0';
            g_ctx.register_identity.identifier_display[1] = 'x';
            if (bytes_to_lowercase_hex(g_ctx.register_identity.identifier_display + 2,
                                       ADDRESS_LENGTH_HEX_STR - 2,
                                       g_ctx.register_identity.identity->identifier,
                                       g_ctx.register_identity.identity->identifier_len) < 0) {
                PRINTF("Failed to format identifier\n");
                APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_identity.identifier_display);
                return NULL;
            }
            g_ctx.current_pair.item = "Address";
            g_ctx.current_pair.value = g_ctx.register_identity.identifier_display;
            break;

        case 3:
            if (APP_MEM_PERMANENT((void **) &g_ctx.register_identity.network_display,
                                  MAX_NETWORK_LEN) == false) {
                PRINTF("Failed to allocate network display buffer\n");
                return NULL;
            }
            if (get_network_as_string_from_chain_id(g_ctx.register_identity.network_display,
                                                    MAX_NETWORK_LEN,
                                                    g_ctx.register_identity.identity->chain_id) ==
                false) {
                PRINTF("Failed to get network name from chain ID\n");
                APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_identity.network_display);
                return NULL;
            }
            g_ctx.current_pair.item = "Network";
            g_ctx.current_pair.value = g_ctx.register_identity.network_display;
            break;

        default:
            PRINTF("Unexpected pair index: %d\n", pairIndex);
            return NULL;
    }
    return &g_ctx.current_pair;
}

/* =========================================================================
 * Edit Contact Name / Edit Scope Name callbacks
 * =========================================================================
 * Display is handled entirely by the SDK; the app only needs to
 * return to idle after the flow completes.
 * =========================================================================
 */

/**
 * @brief Finalize the UI flow for editing a contact name.
 */
void finalize_ui_edit_contact_name(void) {
    address_book_review_cleanup();
    ui_idle();
}

/**
 * @brief Finalize the UI flow for editing a scope name.
 */
void finalize_ui_edit_scope(void) {
    address_book_review_cleanup();
    ui_idle();
}

/* =========================================================================
 * Edit Address (Identifier) callbacks
 * =========================================================================
 */

/**
 * @brief Handle called to finalize the UI flow for editing an Identity.
 */
void finalize_ui_edit_identifier(void) {
    address_book_review_cleanup();
    ui_idle();
}

/**
 * @brief Handle called to validate and store the Edit Address (Identifier) parameters.
 *
 * Validates the new identifier (must be a valid 20-byte Ethereum address),
 * then allocates and converts both identifiers to hex strings for use by
 * get_edit_identifier_tagValue().
 *
 * @param[in] params Contact name, previous identifier, and new identity data
 * @return true if the edit is acceptable, false to reject
 */
bool handle_check_edit_identifier(const edit_identifier_t *params) {
    PRINTF("Inside handle_check_edit_identifier\n");
    if (params == NULL) {
        PRINTF("NULL parameter\n");
        return false;
    }
    // Check if the blockchain family is supported
    if (params->identity.blockchain_family != FAMILY_ETHEREUM) {
        PRINTF("Unsupported blockchain family: %d\n", params->identity.blockchain_family);
        return false;
    }
    // Validate new identifier length (must be 20 bytes for Ethereum)
    if (params->identity.identifier_len != ADDRESS_LENGTH) {
        PRINTF("Invalid new identifier length: %d\n", params->identity.identifier_len);
        return false;
    }
    // Validate previous identifier length
    if (params->old_identifier_len != ADDRESS_LENGTH) {
        PRINTF("Invalid previous identifier length: %d\n", params->old_identifier_len);
        return false;
    }

    // Allocate contact name
    if (APP_MEM_PERMANENT((void **) &g_ctx.edit_identifier.contact_name, CONTACT_NAME_LENGTH) ==
        false) {
        PRINTF("Failed to allocate contact_name\n");
        return false;
    }
    strlcpy(g_ctx.edit_identifier.contact_name, params->identity.contact_name, CONTACT_NAME_LENGTH);

    // Allocate scope
    if (APP_MEM_PERMANENT((void **) &g_ctx.edit_identifier.scope, SCOPE_LENGTH) == false) {
        PRINTF("Failed to allocate scope\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        return false;
    }
    strlcpy(g_ctx.edit_identifier.scope, params->identity.scope, SCOPE_LENGTH);

    // Allocate and format previous identifier as "0x..."
    if (APP_MEM_PERMANENT((void **) &g_ctx.edit_identifier.old_identifier,
                          ADDRESS_LENGTH_HEX_STR) == false) {
        PRINTF("Failed to allocate old_identifier\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
        return false;
    }
    g_ctx.edit_identifier.old_identifier[0] = '0';
    g_ctx.edit_identifier.old_identifier[1] = 'x';
    if (bytes_to_lowercase_hex(g_ctx.edit_identifier.old_identifier + 2,
                               ADDRESS_LENGTH_HEX_STR - 2,
                               params->old_identifier,
                               params->old_identifier_len) < 0) {
        PRINTF("Failed to format previous identifier\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.old_identifier);
        return false;
    }

    // Allocate and format new identifier as "0x..."
    if (APP_MEM_PERMANENT((void **) &g_ctx.edit_identifier.new_identifier,
                          ADDRESS_LENGTH_HEX_STR) == false) {
        PRINTF("Failed to allocate new_identifier\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.old_identifier);
        return false;
    }
    g_ctx.edit_identifier.new_identifier[0] = '0';
    g_ctx.edit_identifier.new_identifier[1] = 'x';
    if (bytes_to_lowercase_hex(g_ctx.edit_identifier.new_identifier + 2,
                               ADDRESS_LENGTH_HEX_STR - 2,
                               params->identity.identifier,
                               params->identity.identifier_len) < 0) {
        PRINTF("Failed to format new identifier\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.old_identifier);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.new_identifier);
        return false;
    }
    address_book_review_set_flow(AB_FLOW_EDIT_IDENTIFIER);
    return true;
}

/**
 * @brief Callback to retrieve a tag-value pair for the Edit Address (Identifier) UI.
 *
 * Pair 0: contact name (unchanged)
 * Pair 1: address name (contact scope) (unchanged)
 * Pair 2: old address (previous identifier - hex with "0x" prefix)
 * Pair 3: new address (new identifier - hex with "0x" prefix)
 *
 * @param[in] pairIndex Index of the tag-value pair
 * @return Pointer to the pair, or NULL if index is invalid
 */
nbgl_contentTagValue_t *get_edit_identifier_tagValue(uint8_t pairIndex) {
    // The pair lives in a union shared with the other flows: clear every field, not
    // just the two set below, or NBGL reads leftover bytes as valueIcon/extension.
    explicit_bzero(&g_ctx.current_pair, sizeof(g_ctx.current_pair));
    switch (pairIndex) {
        case 0:
            g_ctx.current_pair.item = "Contact name";
            g_ctx.current_pair.value = g_ctx.edit_identifier.contact_name;
            break;

        case 1:
            g_ctx.current_pair.item = "Address name";
            g_ctx.current_pair.value = g_ctx.edit_identifier.scope;
            break;

        case 2:
            g_ctx.current_pair.item = "Old address";
            g_ctx.current_pair.value = g_ctx.edit_identifier.old_identifier;
            break;

        case 3:
            g_ctx.current_pair.item = "New address";
            g_ctx.current_pair.value = g_ctx.edit_identifier.new_identifier;
            break;

        default:
            PRINTF("Unexpected pair index: %d\n", pairIndex);
            return NULL;
    }
    return &g_ctx.current_pair;
}

/* =========================================================================
 * Provide-cache update callbacks (called only on confirmed edits)
 * =========================================================================
 */

/**
 * @brief Update cached contacts whose name matches the previous contact name.
 *
 * Called by the SDK only when the user confirmed the name change and the new
 * HMAC proof was successfully sent to the host. Renames all entries in the
 * Provide Contact cache whose contact_name equals @p edit->old_contact_name
 * so that the wallet immediately sees the new name on the next transaction
 * review without requiring a re-provide.
 *
 * @param[in] edit Parsed edit data containing old_contact_name and the new
 *                 contact_name
 */
void on_edit_contact_name_applied(const edit_contact_name_t *edit) {
    if (edit == NULL) {
        return;
    }
    update_contact_name(edit->old_contact_name, edit->contact_name);
}

/**
 * @brief Update the cached contact entry for the previous (identifier, chain_id).
 *
 * Called by the SDK only when the user confirmed the address change and the new
 * HMAC proof was successfully sent to the host. Replaces the identifier in the
 * Provide Contact cache entry that matches (@p edit->old_identifier,
 * @p edit->identity.chain_id) so that the wallet immediately sees the contact
 * name on the next transaction review to the new address without requiring a
 * re-provide.
 *
 * @param[in] edit Parsed edit data containing old_identifier (raw bytes),
 *                 old_identifier_len, and the new identity
 */
void on_edit_identifier_applied(const edit_identifier_t *edit) {
    if (edit == NULL) {
        return;
    }
    update_contact_identifier(edit->old_identifier,
                              edit->identity.chain_id,
                              edit->identity.identifier);
}

/**
 * @brief Update the scope of the cached contact entry for (identifier, chain_id).
 *
 * Called by the SDK only when the user confirmed the scope change and the new
 * HMAC proof was successfully sent to the host. Replaces the scope field in the
 * Provide Contact cache entry that matches (@p edit->identity.identifier,
 * @p edit->identity.chain_id) so that the wallet immediately sees the updated
 * scope on the next transaction review without requiring a re-provide.
 *
 * @param[in] edit Parsed edit data containing old_scope and the new
 *                 identity (identifier, chain_id, new scope)
 */
void on_edit_scope_applied(const edit_scope_t *edit) {
    if (edit == NULL) {
        return;
    }
    update_contact_scope(edit->identity.identifier, edit->identity.chain_id, edit->identity.scope);
}

/* =========================================================================
 * Provide Identity callback
 * =========================================================================
 */

/**
 * @brief Store a verified Address Book Identity contact.
 *
 * Called by the SDK after full cryptographic verification (group_handle,
 * HMAC_PROOF, HMAC_REST). Validates Ethereum-specific constraints, then
 * allocates a node and appends it to the shared contact list.
 *
 * @param[in] contact Verified identity received from the SDK
 * @return true if the contact was accepted and stored, false to reject
 */
bool handle_provide_identity(const identity_t *contact) {
    s_ab_contact *node = NULL;

    PRINTF("Inside handle_provide_identity\n");
    if (contact == NULL) {
        PRINTF("NULL parameter\n");
        return false;
    }
    if (contact->blockchain_family != FAMILY_ETHEREUM) {
        PRINTF("Unsupported blockchain family: %d\n", contact->blockchain_family);
        return false;
    }
    if (contact->identifier_len != ADDRESS_LENGTH) {
        PRINTF("Invalid identifier length: %d (expected %d)\n",
               contact->identifier_len,
               ADDRESS_LENGTH);
        return false;
    }
    if ((contact->chain_id == 0) || (contact->chain_id > MAX_VALID_CHAIN_ID)) {
        PRINTF("Unsupported chain ID: %llu\n", contact->chain_id);
        return false;
    }

    if (get_address_book_contact(contact->chain_id, contact->identifier) != NULL) {
        PRINTF("Contact already stored, ignoring\n");
        return true;
    }

    if (APP_MEM_PERMANENT((void **) &node, sizeof(*node)) == false) {
        PRINTF("Failed to allocate identity\n");
        return false;
    }

    node->type = AB_CONTACT_IDENTITY;
    strlcpy(node->contact_name, contact->contact_name, sizeof(node->contact_name));
    strlcpy(node->scope, contact->scope, sizeof(node->scope));
    memcpy(node->identifier, contact->identifier, ADDRESS_LENGTH);
    node->chain_id = contact->chain_id;

    ab_contact_list_push(node);

    PRINTF("Stored '%s' (chain %llu)\n", node->contact_name, node->chain_id);
    return true;
}

#endif  // HAVE_ADDRESS_BOOK
