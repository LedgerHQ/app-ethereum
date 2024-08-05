#include "crypto_helpers.h"
#include "os_io_seproxyhal.h"
#include "shared_context.h"
#include "common_utils.h"
#include "common_ui.h"
#include "handle_swap_sign_transaction.h"
#include "feature_signTx.h"

unsigned int io_seproxyhal_touch_tx_ok(__attribute__((unused)) const bagl_element_t *e) {
    uint32_t info = 0;
    int err;
    if (bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                            tmpCtx.transactionContext.bip32.path,
                                            tmpCtx.transactionContext.bip32.length,
                                            CX_RND_RFC6979 | CX_LAST,
                                            CX_SHA256,
                                            tmpCtx.transactionContext.hash,
                                            sizeof(tmpCtx.transactionContext.hash),
                                            G_io_apdu_buffer + 1,
                                            G_io_apdu_buffer + 1 + 32,
                                            &info) != CX_OK) {
        THROW(0x6F00);
    }

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
    G_io_apdu_buffer[1 + 64] = 0x90;
    G_io_apdu_buffer[1 + 64 + 1] = 0x00;

    // Send back the response, do not restart the event loop
    err = io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 1 + 64 + 2);
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
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}

unsigned int io_seproxyhal_touch_tx_cancel(__attribute__((unused)) const bagl_element_t *e) {
    reset_app_context();
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}

unsigned int io_seproxyhal_touch_data_ok(__attribute__((unused)) const bagl_element_t *e) {
    parserStatus_e txResult = USTREAM_FINISHED;
    txResult = continueTx(&txContext);
    switch (txResult) {
        case USTREAM_SUSPENDED:
            break;
        case USTREAM_FINISHED:
            break;
        case USTREAM_PROCESSING:
            io_seproxyhal_send_status(0x9000);
            ui_idle();
            break;
        case USTREAM_FAULT:
            reset_app_context();
            io_seproxyhal_send_status(0x6A80);
            ui_idle();
            break;
        default:
            PRINTF("Unexpected parser status\n");
            reset_app_context();
            io_seproxyhal_send_status(0x6A80);
            ui_idle();
    }

    if (txResult == USTREAM_FINISHED) {
        finalizeParsing();
    }

    return 0;
}

unsigned int io_seproxyhal_touch_data_cancel(__attribute__((unused)) const bagl_element_t *e) {
    reset_app_context();
    io_seproxyhal_send_status(0x6985);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}
