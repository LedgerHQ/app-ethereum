/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file handle_identity.c
 * @brief Coin-app callbacks for Identity flows
 *
 * Register Identity callbacks:
 *  - handle_check_identity(): validate the parsed identity data
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
 */

#include "identity.h"
#include "common_utils.h"
#include "network.h"
#include "ui_nbgl.h"
#include "app_mem_utils.h"
#include "os_utils.h"

#if defined(HAVE_ADDRESS_BOOK)

/* Private defines -----------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/**
 * @brief Context for Register Identity flow
 */
typedef struct {
    identity_t *identity;
    char *identifier_display;
    char *network_display;
} register_identity_ctx_t;

/**
 * @brief Context for Edit Identifier flow
 */
typedef struct {
    char *contact_name;
    char *scope;
    char *old_identifier;
    char *new_identifier;
} edit_identifier_ctx_t;

/**
 * @brief Global context for all Identity-related UI flows
 */
typedef struct {
    union {
        register_identity_ctx_t register_identity;
        edit_identifier_ctx_t edit_identifier;
    };
} identity_ui_ctx_t;

/* Private variables ---------------------------------------------------------*/
static identity_ui_ctx_t g_ctx = {0};

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
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_identity.identifier_display);
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_identity.network_display);
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.register_identity.identity);
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
bool handle_check_identity(identity_t *params) {
    char checksummed_address[ADDRESS_LENGTH_STR] = {0};

    PRINTF("Inside handle_check_identity\n");
    if (params == NULL) {
        PRINTF("params is NULL\n");
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

    g_ctx.register_identity.identity = APP_MEM_ALLOC(sizeof(identity_t));
    if (g_ctx.register_identity.identity == NULL) {
        PRINTF("Failed to allocate identity\n");
        return false;
    }
    memmove(g_ctx.register_identity.identity, params, sizeof(identity_t));
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
    static nbgl_contentTagValue_t currentPair = {0};

    switch (pairIndex) {
        case 0:
            currentPair.item = "Contact name";
            currentPair.value = g_ctx.register_identity.identity->contact_name;
            break;

        case 1:
            currentPair.item = "Address name";
            currentPair.value = g_ctx.register_identity.identity->scope;
            break;

        case 2:
            g_ctx.register_identity.identifier_display = APP_MEM_ALLOC(ADDRESS_LENGTH_HEX_STR);
            if (g_ctx.register_identity.identifier_display == NULL) {
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
            currentPair.item = "Address";
            currentPair.value = g_ctx.register_identity.identifier_display;
            break;

        case 3:
            g_ctx.register_identity.network_display = APP_MEM_ALLOC(MAX_NETWORK_LEN);
            if (g_ctx.register_identity.network_display == NULL) {
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
            currentPair.item = "Network";
            currentPair.value = g_ctx.register_identity.network_display;
            break;

        default:
            PRINTF("Unexpected pair index: %d\n", pairIndex);
            return NULL;
    }
    return &currentPair;
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
    ui_idle();
}

/**
 * @brief Finalize the UI flow for editing a scope name.
 */
void finalize_ui_edit_scope(void) {
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
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.old_identifier);
    APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.new_identifier);
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
    if (params == NULL) {
        PRINTF("handle_check_edit_identifier: NULL parameter\n");
        return false;
    }
    // Check if the blockchain family is supported
    if (params->identity.blockchain_family != FAMILY_ETHEREUM) {
        PRINTF("handle_check_edit_identifier: unsupported blockchain family: %d\n",
               params->identity.blockchain_family);
        return false;
    }
    // Validate new identifier length (must be 20 bytes for Ethereum)
    if (params->identity.identifier_len != ADDRESS_LENGTH) {
        PRINTF("handle_check_edit_identifier: invalid new identifier length: %d\n",
               params->identity.identifier_len);
        return false;
    }
    // Validate previous identifier length
    if (params->previous_identifier_len != ADDRESS_LENGTH) {
        PRINTF("handle_check_edit_identifier: invalid previous identifier length: %d\n",
               params->previous_identifier_len);
        return false;
    }

    // Allocate contact name
    g_ctx.edit_identifier.contact_name = APP_MEM_ALLOC(CONTACT_NAME_LENGTH);
    if (g_ctx.edit_identifier.contact_name == NULL) {
        PRINTF("Failed to allocate contact_name\n");
        return false;
    }
    strncpy(g_ctx.edit_identifier.contact_name,
            params->identity.contact_name,
            CONTACT_NAME_LENGTH - 1);

    // Allocate scope
    g_ctx.edit_identifier.scope = APP_MEM_ALLOC(SCOPE_LENGTH);
    if (g_ctx.edit_identifier.scope == NULL) {
        PRINTF("Failed to allocate scope\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        return false;
    }
    strncpy(g_ctx.edit_identifier.scope, params->identity.scope, SCOPE_LENGTH - 1);

    // Allocate and format previous identifier as "0x..."
    g_ctx.edit_identifier.old_identifier = APP_MEM_ALLOC(ADDRESS_LENGTH_HEX_STR);
    if (g_ctx.edit_identifier.old_identifier == NULL) {
        PRINTF("Failed to allocate old_identifier\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
        return false;
    }
    g_ctx.edit_identifier.old_identifier[0] = '0';
    g_ctx.edit_identifier.old_identifier[1] = 'x';
    if (bytes_to_lowercase_hex(g_ctx.edit_identifier.old_identifier + 2,
                               ADDRESS_LENGTH_HEX_STR - 2,
                               params->previous_identifier,
                               params->previous_identifier_len) < 0) {
        PRINTF("handle_check_edit_identifier: failed to format previous identifier\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.old_identifier);
        return false;
    }

    // Allocate and format new identifier as "0x..."
    g_ctx.edit_identifier.new_identifier = APP_MEM_ALLOC(ADDRESS_LENGTH_HEX_STR);
    if (g_ctx.edit_identifier.new_identifier == NULL) {
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
        PRINTF("handle_check_edit_identifier: failed to format new identifier\n");
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.contact_name);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.scope);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.old_identifier);
        APP_MEM_FREE_AND_NULL((void **) &g_ctx.edit_identifier.new_identifier);
        return false;
    }
    return true;
}

/**
 * @brief Callback to retrieve a tag-value pair for the Edit Address (Identifier) UI.
 *
 * Pair 0: contact name (unchanged)
 * Pair 1: address name (contact scope) (unchanged)
 * Pair 2: previous address (previous identifier - hex with "0x" prefix)
 * Pair 3: new address (new identifier - hex with "0x" prefix)
 *
 * @param[in] pairIndex Index of the tag-value pair
 * @return Pointer to the pair, or NULL if index is invalid
 */
nbgl_contentTagValue_t *get_edit_identifier_tagValue(uint8_t pairIndex) {
    static nbgl_contentTagValue_t currentPair = {0};

    switch (pairIndex) {
        case 0:
            currentPair.item = "Contact name";
            currentPair.value = g_ctx.edit_identifier.contact_name;
            break;

        case 1:
            currentPair.item = "Address name";
            currentPair.value = g_ctx.edit_identifier.scope;
            break;

        case 2:
            currentPair.item = "Previous address";
            currentPair.value = g_ctx.edit_identifier.old_identifier;
            break;

        case 3:
            currentPair.item = "New address";
            currentPair.value = g_ctx.edit_identifier.new_identifier;
            break;

        default:
            PRINTF("Unexpected pair index: %d\n", pairIndex);
            return NULL;
    }
    return &currentPair;
}

#endif  // HAVE_ADDRESS_BOOK
