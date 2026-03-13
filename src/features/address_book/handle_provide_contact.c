/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file handle_provide_contact.c
 * @brief Coin-app callback for the Provide Contact (P1=0x20) command.
 *
 * The SDK verifies the TLV payload, group_handle, HMAC_PROOF and HMAC_REST
 * before calling handle_provide_contact().  This file only stores the
 * verified contact for later use during transaction review.
 *
 * Lookup:  get_address_book_contact(chain_id, raw_addr)
 * Cleanup: address_book_contact_cleanup()
 */

#include <string.h>
#include "handle_provide_contact.h"
#include "identity.h"
#include "common_utils.h"   // ADDRESS_LENGTH
#include "chain_config.h"   // MAX_VALID_CHAIN_ID
#include "app_mem_utils.h"  // APP_MEM_ALLOC, APP_MEM_FREE
#include "os_utils.h"       // PRINTF

#if defined(HAVE_ADDRESS_BOOK)

/* Private variables ---------------------------------------------------------*/
static s_ab_contact *g_contact_list = NULL;

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Free a single contact node.
 *
 * @param[in] node Node to free
 */
static void delete_contact(s_ab_contact *node) {
    APP_MEM_FREE(node);
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Release all stored Address Book contacts.
 *
 * Iterates the linked list and frees every node. Should be called when
 * the application returns to idle so that the next signing flow starts
 * with an empty contact list.
 */
void address_book_contact_cleanup(void) {
    flist_clear((flist_node_t **) &g_contact_list, (f_list_node_del) &delete_contact);
}

/**
 * @brief Look up a stored contact by chain ID and address.
 *
 * Iterates the internal linked list and returns the first entry whose
 * chain_id and raw 20-byte address both match.
 *
 * @param[in] chain_id Chain ID of the transaction being signed
 * @param[in] addr     Raw 20-byte Ethereum address to look up
 * @return Pointer to the matching contact, or NULL if not found
 */
const s_ab_contact *get_address_book_contact(uint64_t chain_id, const uint8_t *addr) {
    for (s_ab_contact *tmp = g_contact_list; tmp != NULL;
         tmp = (s_ab_contact *) ((flist_node_t *) tmp)->next) {
        if ((tmp->chain_id == chain_id) && (memcmp(tmp->identifier, addr, ADDRESS_LENGTH) == 0)) {
            return tmp;
        }
    }
    return NULL;
}

/**
 * @brief Store a verified Address Book contact.
 *
 * Called by the SDK after full cryptographic verification (group_handle,
 * HMAC_PROOF, HMAC_REST). Validates Ethereum-specific constraints, then
 * allocates a node and appends it to the internal contact list.
 *
 * @param[in] contact Verified identity received from the SDK
 * @return true if the contact was accepted and stored, false to reject
 */
bool handle_provide_contact(const identity_t *contact) {
    s_ab_contact *node = NULL;

    if (contact == NULL) {
        PRINTF("handle_provide_contact: NULL parameter\n");
        return false;
    }
    // Only accept Ethereum-family contacts
    if (contact->blockchain_family != FAMILY_ETHEREUM) {
        PRINTF("handle_provide_contact: unsupported blockchain family: %d\n",
               contact->blockchain_family);
        return false;
    }
    // Identifier must be exactly 20 bytes (raw Ethereum address)
    if (contact->identifier_len != ADDRESS_LENGTH) {
        PRINTF("handle_provide_contact: invalid identifier length: %d (expected %d)\n",
               contact->identifier_len,
               ADDRESS_LENGTH);
        return false;
    }
    // Chain ID must be non-zero and in the valid range
    if ((contact->chain_id == 0) || (contact->chain_id > MAX_VALID_CHAIN_ID)) {
        PRINTF("handle_provide_contact: invalid chain ID: %llu\n", contact->chain_id);
        return false;
    }

    node = APP_MEM_ALLOC(sizeof(*node));
    if (node == NULL) {
        PRINTF("handle_provide_contact: allocation failed\n");
        return false;
    }
    memset(node, 0, sizeof(*node));

    strncpy(node->contact_name, contact->contact_name, sizeof(node->contact_name) - 1);
    strncpy(node->scope, contact->scope, sizeof(node->scope) - 1);
    memcpy(node->identifier, contact->identifier, ADDRESS_LENGTH);
    node->chain_id = contact->chain_id;

    flist_push_back((flist_node_t **) &g_contact_list, (flist_node_t *) node);

    PRINTF("handle_provide_contact: stored '%s' (chain %llu)\n",
           node->contact_name,
           node->chain_id);
    return true;
}

#endif  // HAVE_ADDRESS_BOOK
