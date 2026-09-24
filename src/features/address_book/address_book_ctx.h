/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file address_book_ctx.h
 * @brief Shared UI context for all Address Book flows.
 *
 * Identity and Ledger Account flows are mutually exclusive (only one UI flow
 * runs at a time), so their per-flow context structs are kept in a union to
 * minimise RAM usage.
 *
 * The unified contact list (g_ab_contact_list in handle_contacts.c) is
 * intentionally NOT included here: it holds persistent state that survives
 * across UI flows and must not be reset between operations.
 */

#pragma once

#if defined(HAVE_ADDRESS_BOOK)

#include "identity.h"
#include "nbgl_use_case.h"  // nbgl_contentTagValue_t (via nbgl_content.h), nbgl_warningDetails_t

/* ---------------------------------------------------------------------------
 * Identity UI contexts
 * --------------------------------------------------------------------------- */

typedef struct {
    identity_t *identity;
    char *identifier_display;
    char *network_display;
} register_identity_ctx_t;

typedef struct {
    char *contact_name;
    char *scope;
    char *old_identifier;
    char *new_identifier;
} edit_identifier_ctx_t;

typedef struct {
    union {
        register_identity_ctx_t register_identity;
        edit_identifier_ctx_t edit_identifier;
    };
    nbgl_contentTagValue_t current_pair;
} identity_ui_ctx_t;

#if defined(HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT)

#include "ledger_account.h"

/* ---------------------------------------------------------------------------
 * Ledger Account UI contexts
 * --------------------------------------------------------------------------- */

/** Number of hex chars shown on each side of the middle-truncated address */
#define ADDR_SHORT_HEX_LEN 4
/** "0x" + ADDR_SHORT_HEX_LEN + "..." + ADDR_SHORT_HEX_LEN + '\0' */
#define ADDR_DISPLAY_SHORT_SIZE (2 + ADDR_SHORT_HEX_LEN + 3 + ADDR_SHORT_HEX_LEN + 1)

typedef struct {
    char *address_display;
    char *network_display;
#ifdef SCREEN_SIZE_WALLET
    char address_display_short[ADDR_DISPLAY_SHORT_SIZE];  ///< "0x1234...5678"
#endif
    const char *texts[1];
    const char *subTexts[1];
    nbgl_warningDetails_t details;
} register_ledger_account_ctx_t;

typedef struct {
    ledger_account_t *ledger_account;
    register_ledger_account_ctx_t register_account;
} ledger_account_ui_ctx_t;

#endif  // HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT

/* ---------------------------------------------------------------------------
 * Shared UI context union — only one flow is active at a time
 * --------------------------------------------------------------------------- */

typedef union {
    identity_ui_ctx_t identity;
#if defined(HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT)
    ledger_account_ui_ctx_t ledger_account;
#endif
} ab_ui_ctx_t;

extern ab_ui_ctx_t g_ab_ctx;

/** Which flow owns the buffers currently referenced by g_ab_ctx */
typedef enum {
    AB_FLOW_NONE,
    AB_FLOW_REGISTER_IDENTITY,
    AB_FLOW_EDIT_IDENTIFIER,
#if defined(HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT)
    AB_FLOW_LEDGER_ACCOUNT,
#endif
} ab_flow_e;

/**
 * Record which flow owns g_ab_ctx.
 *
 * Set once the flow's buffers are allocated, so a cleanup reached from anywhere
 * releases the right union member.
 *
 * @param[in] flow the flow taking ownership, AB_FLOW_NONE to release it
 */
void address_book_review_set_flow(ab_flow_e flow);

/**
 * Release the review state: free the owning flow's buffers and scrub the context.
 *
 * Leaving them behind would let the next flow read them through the union, as
 * another flow's pointers. The screen is left untouched.
 */
void address_book_review_cleanup(void);

#endif  // HAVE_ADDRESS_BOOK
