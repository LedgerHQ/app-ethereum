/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"  // ADDRESS_LENGTH
#include "identity.h"      // CONTACT_NAME_LENGTH, SCOPE_LENGTH
#include "lists.h"         // flist_node_t

#if defined(HAVE_ADDRESS_BOOK)

/**
 * @brief A stored Address Book contact (Ethereum-specific).
 *
 * The `_list` member must remain first so that `flist_*` helpers can cast
 * freely between `s_ab_contact *` and `flist_node_t *`.
 */
typedef struct {
    flist_node_t _list;
    char contact_name[CONTACT_NAME_LENGTH];
    char scope[SCOPE_LENGTH];
    uint8_t identifier[ADDRESS_LENGTH];  ///< 20-byte raw address
    uint64_t chain_id;
} s_ab_contact;

/**
 * @brief Look up a stored contact by chain ID and address.
 *
 * Iterates the internal linked list and returns the first entry whose
 * chain_id and raw 20-byte address match the given parameters.
 *
 * @param[in] chain_id Chain ID of the transaction being signed
 * @param[in] addr     Raw 20-byte Ethereum address to look up
 * @return Pointer to the matching contact, or NULL if not found
 */
const s_ab_contact *get_address_book_contact(uint64_t chain_id, const uint8_t *addr);

/**
 * @brief Release all stored Address Book contacts.
 *
 * Frees every node in the internal linked list. Should be called when
 * the application returns to idle (e.g. from finalize_ui_* callbacks or
 * after a signing flow completes).
 */
void address_book_contact_cleanup(void);

#endif  // HAVE_ADDRESS_BOOK
