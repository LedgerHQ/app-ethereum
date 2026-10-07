/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#if defined(HAVE_ADDRESS_BOOK)

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"  // ADDRESS_LENGTH
#include "identity.h"      // CONTACT_NAME_LENGTH, SCOPE_LENGTH
#include "lists.h"         // flist_node_t

/**
 * @brief Type of a stored Address Book contact.
 */
typedef enum {
    AB_CONTACT_IDENTITY,
    AB_CONTACT_LEDGER_ACCOUNT,
} ab_contact_type_e;

/**
 * @brief A stored Address Book contact (Ethereum-specific).
 *
 * The `_list` member must remain first so that `flist_*` helpers can cast
 * freely between `s_ab_contact *` and `flist_node_t *`.
 */
typedef struct {
    flist_node_t _list;
    ab_contact_type_e type;
    char contact_name[CONTACT_NAME_LENGTH];
    char scope[SCOPE_LENGTH];
    uint8_t identifier[ADDRESS_LENGTH];  ///< 20-byte raw address
    uint64_t chain_id;
} s_ab_contact;

void ab_contact_list_push(s_ab_contact *node);

const s_ab_contact *get_address_book_contact(uint64_t chain_id, const uint8_t *addr);

void address_book_contact_cleanup(void);

void update_contact_name(const char *old_name, const char *new_name);

void update_contact_identifier(const uint8_t *old_addr, uint64_t chain_id, const uint8_t *new_addr);

void update_contact_scope(const uint8_t *addr, uint64_t chain_id, const char *new_scope);

void update_ledger_account_contact_name(const uint8_t *addr,
                                        uint64_t chain_id,
                                        const char *new_name);

#endif  // HAVE_ADDRESS_BOOK
