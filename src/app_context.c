#include <stdint.h>
#include <string.h>

#include "shared_context.h"
#include "eip712_v1_context.h"  // eip712_v1_context_deinit
#include "bip32_utils.h"
#include "bip32.h"
#include "eth_swap_utils.h"
#include "feature_sign_tx.h"
#include "manage_asset_info.h"
#include "sign_message.h"
#include "trusted_name.h"
#include "enum_value.h"
#include "map_entry.h"
#include "proxy_info.h"
#ifdef HAVE_TRANSACTION_CHECKS
#include "cmd_get_tx_simulation.h"  // clear_tx_simulation
#endif
#include "tx_ctx.h"
#include "ui_utils.h"
#include "app_mem_utils.h"
#include "cmd_safe_account.h"
#include "cmd_get_gating.h"
#include "network_info.h"
#include "main_std_app.h"  // app_exit()

#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#include "address_book_ctx.h"
#endif

#ifdef HAVE_ETH2
#include "withdrawal_index.h"
#endif

// Tear down every per-flow global.
//
// `scrub_strings` wipes the `strings` union, which holds the addresses and amounts of the
// transaction under review. Only a caller that is ending a user-facing flow should ask for
// it: on an initialization path there is nothing to protect yet, and in library (swap) mode
// copy_transaction_parameters() has already staged the values promised by the Exchange app
// in there before app_init() runs.
void reset_app_context_ex(bool scrub_strings) {
    // Every reset reachable during a swap session is an error path: main.c's error
    // branch, report_finalize_error() and the plugin UI failure. None of them may leave
    // the session alive with its guards cleared, so the session ends here instead.
    swap_abort_session();

    // Same reasoning for a reset happening once the swap flow is under way:
    // finalize_parsing_helper() still has to compare those staged values against the
    // parsed transaction. Read the flag before it is cleared further down.
    if (G_called_from_swap) {
        scrub_strings = false;
    }

    if (appState == APP_STATE_SIGNING_MESSAGE) {
        message_cleanup();
    }
    eip712_v1_context_deinit();
    G_called_from_swap = false;
    G_swap_response_ready = false;
    G_swap_checked = false;
    G_swap_tx_had_calldata = false;
    G_swap_calldata_validated = false;
    pluginType = PLUGIN_TYPE_NONE;
#ifdef HAVE_ETH2
    eth2WithdrawalIndex = 0;
#endif
    memset((uint8_t *) &tmpCtx, 0, sizeof(tmpCtx));
    memset((uint8_t *) &dataContext, 0, sizeof(dataContext));
    forget_known_assets();
    if (txContext.store_calldata) {
        gcs_cleanup();
    }
    trusted_name_cleanup();
    enum_value_cleanup();
    map_entry_cleanup();
    // Release the tx-signing keccak context.
    if (g_tx_hash_ctx != NULL) {
        APP_MEM_FREE_AND_NULL((void **) &g_tx_hash_ctx);
    }
    memset((uint8_t *) &txContext, 0, sizeof(txContext));
    memset((uint8_t *) &tmpContent, 0, sizeof(tmpContent));
    clear_safe_account();
#ifdef HAVE_TRANSACTION_CHECKS
    clear_tx_simulation();
#endif
    ui_all_cleanup();
#ifdef HAVE_ADDRESS_BOOK
    // A review the SDK displayed on our behalf may have been cut short by an error,
    // leaving its buffers allocated and g_ab_ctx holding stale pointers.
    address_book_review_cleanup();
#endif
    proxy_cleanup();
    clear_gating();
    if (scrub_strings) {
        explicit_bzero((uint8_t *) &strings, sizeof(strings));
    }
    appState = APP_STATE_IDLE;
}

void reset_app_context(void) {
    reset_app_context_ex(true);
}

void app_quit(void) {
    network_info_cleanup(NULL);
#ifdef HAVE_ADDRESS_BOOK
    address_book_contact_cleanup();
#endif  // HAVE_ADDRESS_BOOK
    reset_app_context();
    app_exit();
}

const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    if (*dataLength < 1) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    bip32->length = *dataBuffer;

    dataBuffer++;
    (*dataLength)--;

    if (*dataLength < sizeof(uint32_t) * (bip32->length)) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    if (bip32_path_read(dataBuffer, (size_t) dataLength, bip32->path, (size_t) bip32->length) ==
        false) {
        PRINTF("Invalid Path data\n");
        return NULL;
    }
    dataBuffer += bip32->length * sizeof(uint32_t);
    *dataLength -= bip32->length * sizeof(uint32_t);

    return dataBuffer;
}
