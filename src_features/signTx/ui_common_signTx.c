#include "crypto_helpers.h"
#include "os_io_seproxyhal.h"
#include "shared_context.h"
#include "common_utils.h"
#include "common_ui.h"
#include "handle_swap_sign_transaction.h"
#include "feature_signTx.h"
#include "apdu_constants.h"

uint32_t io_seproxyhal_touch_tx_ok(void) {
    uint32_t info = 0;
    int err = 0;
    CX_ASSERT(bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                                  tmpCtx.transactionContext.bip32.path,
                                                  tmpCtx.transactionContext.bip32.length,
                                                  CX_RND_RFC6979 | CX_LAST,
                                                  CX_SHA256,
                                                  tmpCtx.transactionContext.hash,
                                                  sizeof(tmpCtx.transactionContext.hash),
                                                  G_io_apdu_buffer + 1,
                                                  G_io_apdu_buffer + 1 + 32,
                                                  &info));

    if (txContext.txType == EIP1559 || txContext.txType == EIP2930) {
        if (info & CX_ECCINFO_PARITY_ODD) {
            G_io_apdu_buffer[0] = 1;
        } else {
            G_io_apdu_buffer[0] = 0;
        }
    } else {
        // Parity is present in the sequence tag in the legacy API
        if (tmpContent.txContent.vLength == 0) {
            // Legacy API
            G_io_apdu_buffer[0] = 27;
        } else {
            // New API
            // Note that this is wrong for a large v, but ledgerjs will recover.

            // Taking only the 4 highest bytes to not introduce breaking changes. In the future,
            // this should be updated.
            uint32_t v = (uint32_t) u64_from_BE(tmpContent.txContent.v,
                                                MIN(4, tmpContent.txContent.vLength));
            G_io_apdu_buffer[0] = (v * 2) + 35;
        }
        if (info & CX_ECCINFO_PARITY_ODD) {
            G_io_apdu_buffer[0]++;
        }
        if (info & CX_ECCINFO_xGTn) {
            G_io_apdu_buffer[0] += 2;
        }
    }

    // Write status code at parity_byte + r + s
    // Send back the response, do not restart the event loop
    err = io_seproxyhal_send_status(APDU_RESPONSE_OK, 65, false, false);
    if (G_called_from_swap) {
        PRINTF("G_called_from_swap\n");

        // If we are in swap mode and have validated a TX, we send it and immediately quit
        if (err == 0) {
            finalize_exchange_sign_transaction(true);
        } else {
            PRINTF("Unrecoverable\n");
            app_exit();
        }
    }
    reset_app_context();
    return 0;  // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_cancel(void) {
    return io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, false);
}

unsigned int io_seproxyhal_touch_data_ok(void) {
    unsigned int err = 0;
    parserStatus_e pstatus = continueTx(&txContext);
    uint16_t sw = handle_parsing_status(pstatus);
    if ((pstatus != USTREAM_SUSPENDED) && (pstatus != USTREAM_FINISHED)) {
        err = io_seproxyhal_send_status(sw, 0, sw != APDU_RESPONSE_OK, true);
    }
    return err;
}

unsigned int io_seproxyhal_touch_data_cancel(void) {
    return io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, true);
}
