/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file handle_contacts.c
 * @brief Unified Address Book contact list management.
 *
 * Maintains a single list for both Identity and Ledger Account contacts.
 * The two types are distinguished by the `type` field of s_ab_contact.
 *
 * Lookup:  get_address_book_contact(chain_id, raw_addr)
 * Cleanup: address_book_contact_cleanup()
 */

#if defined(HAVE_ADDRESS_BOOK)

#include <string.h>
#include "handle_contacts.h"
#include "common_utils.h"   // ADDRESS_LENGTH
#include "app_mem_utils.h"  // APP_MEM_FREE
#include "os_utils.h"       // PRINTF

/* Private variables ---------------------------------------------------------*/
static s_ab_contact *g_ab_contact_list = NULL;

/* Private functions ---------------------------------------------------------*/

static void delete_contact(s_ab_contact *node) {
    APP_MEM_FREE(node);
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Append an already-allocated contact node to the shared contact list.
 *
 * Ownership of @p node transfers to the list; the caller must not free it.
 *
 * @param[in] node Fully populated contact node to append
 */
void ab_contact_list_push(s_ab_contact *node) {
    flist_push_back((flist_node_t **) &g_ab_contact_list, (flist_node_t *) node);
}

/**
 * @brief Release all stored Address Book contacts (Identity and Ledger Account).
 */
void address_book_contact_cleanup(void) {
    flist_clear((flist_node_t **) &g_ab_contact_list, (f_list_node_del) &delete_contact);
}

/**
 * @brief Rename all Identity cache entries whose contact_name equals @p old_name.
 *
 * @param[in] old_name Old contact name to match (no-op if NULL)
 * @param[in] new_name New contact name to apply (no-op if NULL)
 */
void update_contact_name(const char *old_name, const char *new_name) {
    if (old_name == NULL || new_name == NULL) {
        return;
    }
    for (s_ab_contact *node = g_ab_contact_list; node != NULL;
         node = (s_ab_contact *) ((flist_node_t *) node)->next) {
        if ((node->type == AB_CONTACT_IDENTITY) &&
            (strncmp(node->contact_name, old_name, CONTACT_NAME_LENGTH) == 0)) {
            strlcpy(node->contact_name, new_name, sizeof(node->contact_name));
            PRINTF("update_contact_name: '%s' → '%s'\n", old_name, new_name);
        }
    }
}

/**
 * @brief Update the identifier of the Identity cache entry matching (old_addr, chain_id).
 *
 * @param[in] old_addr Old raw 20-byte Ethereum address (no-op if NULL)
 * @param[in] chain_id Chain ID to match
 * @param[in] new_addr New raw 20-byte Ethereum address (no-op if NULL)
 */
void update_contact_identifier(const uint8_t *old_addr,
                               uint64_t chain_id,
                               const uint8_t *new_addr) {
    if (old_addr == NULL || new_addr == NULL) {
        return;
    }
    for (s_ab_contact *node = g_ab_contact_list; node != NULL;
         node = (s_ab_contact *) ((flist_node_t *) node)->next) {
        if ((node->type == AB_CONTACT_IDENTITY) && (node->chain_id == chain_id) &&
            (memcmp(node->identifier, old_addr, ADDRESS_LENGTH) == 0)) {
            memcpy(node->identifier, new_addr, ADDRESS_LENGTH);
            PRINTF("update_contact_identifier: updated identifier for chain %llu\n", chain_id);
            return;  // (identifier, chain_id) is unique in the list
        }
    }
}

/**
 * @brief Update the scope of the Identity cache entry matching (addr, chain_id).
 *
 * @param[in] addr      Raw 20-byte Ethereum address to match (no-op if NULL)
 * @param[in] chain_id  Chain ID to match
 * @param[in] new_scope New scope string (no-op if NULL)
 */
void update_contact_scope(const uint8_t *addr, uint64_t chain_id, const char *new_scope) {
    if (addr == NULL || new_scope == NULL) {
        return;
    }
    for (s_ab_contact *node = g_ab_contact_list; node != NULL;
         node = (s_ab_contact *) ((flist_node_t *) node)->next) {
        if ((node->type == AB_CONTACT_IDENTITY) && (node->chain_id == chain_id) &&
            (memcmp(node->identifier, addr, ADDRESS_LENGTH) == 0)) {
            strlcpy(node->scope, new_scope, sizeof(node->scope));
            PRINTF("update_contact_scope: updated scope for chain %llu\n", chain_id);
            return;  // (identifier, chain_id) is unique in the list
        }
    }
}

/**
 * @brief Update the contact_name of the Ledger Account entry matching (addr, chain_id).
 *
 * @param[in] addr     Raw 20-byte Ethereum address to match (no-op if NULL)
 * @param[in] chain_id Chain ID to match
 * @param[in] new_name New account name to apply (no-op if NULL)
 */
void update_ledger_account_contact_name(const uint8_t *addr,
                                        uint64_t chain_id,
                                        const char *new_name) {
    if (addr == NULL || new_name == NULL) {
        return;
    }
    for (s_ab_contact *node = g_ab_contact_list; node != NULL;
         node = (s_ab_contact *) ((flist_node_t *) node)->next) {
        if ((node->type == AB_CONTACT_LEDGER_ACCOUNT) && (node->chain_id == chain_id) &&
            (memcmp(node->identifier, addr, ADDRESS_LENGTH) == 0)) {
            strlcpy(node->contact_name, new_name, sizeof(node->contact_name));
            PRINTF("update_ledger_account_contact_name: updated name for chain %llu\n", chain_id);
            return;  // (addr, chain_id) is unique in the list
        }
    }
}

/**
 * @brief Look up a stored contact by chain ID and address (any type).
 *
 * @param[in] chain_id Chain ID of the transaction being signed
 * @param[in] addr     Raw 20-byte Ethereum address to look up
 * @return Pointer to the matching contact, or NULL if not found
 */
const s_ab_contact *get_address_book_contact(uint64_t chain_id, const uint8_t *addr) {
    for (s_ab_contact *tmp = g_ab_contact_list; tmp != NULL;
         tmp = (s_ab_contact *) ((flist_node_t *) tmp)->next) {
        if ((tmp->chain_id == chain_id) && (memcmp(tmp->identifier, addr, ADDRESS_LENGTH) == 0)) {
            return tmp;
        }
    }
    return NULL;
}

#endif  // HAVE_ADDRESS_BOOK
