/* SPDX-FileCopyrightText: © 2026 Ledger SAS */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include "address_book_ctx.h"
#include "app_mem_utils.h"

#if defined(HAVE_ADDRESS_BOOK)

ab_ui_ctx_t g_ab_ctx = {0};

static ab_flow_e s_ab_flow = AB_FLOW_NONE;

void address_book_review_set_flow(ab_flow_e flow) {
    s_ab_flow = flow;
}

void address_book_review_cleanup(void) {
    switch (s_ab_flow) {
        case AB_FLOW_REGISTER_IDENTITY:
            APP_MEM_FREE_AND_NULL(
                (void **) &g_ab_ctx.identity.register_identity.identifier_display);
            APP_MEM_FREE_AND_NULL((void **) &g_ab_ctx.identity.register_identity.network_display);
            APP_MEM_FREE_AND_NULL((void **) &g_ab_ctx.identity.register_identity.identity);
            break;
        case AB_FLOW_EDIT_IDENTIFIER:
            APP_MEM_FREE_AND_NULL((void **) &g_ab_ctx.identity.edit_identifier.contact_name);
            APP_MEM_FREE_AND_NULL((void **) &g_ab_ctx.identity.edit_identifier.scope);
            APP_MEM_FREE_AND_NULL((void **) &g_ab_ctx.identity.edit_identifier.old_identifier);
            APP_MEM_FREE_AND_NULL((void **) &g_ab_ctx.identity.edit_identifier.new_identifier);
            break;
#if defined(HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT)
        case AB_FLOW_LEDGER_ACCOUNT:
            APP_MEM_FREE_AND_NULL(
                (void **) &g_ab_ctx.ledger_account.register_account.address_display);
            APP_MEM_FREE_AND_NULL(
                (void **) &g_ab_ctx.ledger_account.register_account.network_display);
            APP_MEM_FREE_AND_NULL((void **) &g_ab_ctx.ledger_account.ledger_account);
            break;
#endif
        case AB_FLOW_NONE:
            break;
    }
    s_ab_flow = AB_FLOW_NONE;
    explicit_bzero(&g_ab_ctx, sizeof(g_ab_ctx));
}

#endif  // HAVE_ADDRESS_BOOK
