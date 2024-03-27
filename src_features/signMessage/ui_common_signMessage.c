#include "os_io_seproxyhal.h"
#include "apdu_constants.h"
#include "lib_standard_app/crypto_helpers.h"
#include "common_ui.h"

unsigned int io_seproxyhal_touch_signMessage_ok(void) {
    uint32_t tx = 0;
    unsigned int info = 0;
    if (bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                            tmpCtx.messageSigningContext.bip32.path,
                                            tmpCtx.messageSigningContext.bip32.length,
                                            CX_RND_RFC6979 | CX_LAST,
                                            CX_SHA256,
                                            tmpCtx.messageSigningContext.hash,
                                            sizeof(tmpCtx.messageSigningContext.hash),
                                            G_io_apdu_buffer + 1,
                                            G_io_apdu_buffer + 1 + 32,
                                            &info) != CX_OK) {
        THROW(APDU_RESPONSE_UNKNOWN);
    }
    G_io_apdu_buffer[0] = 27;
    if (info & CX_ECCINFO_PARITY_ODD) {
        G_io_apdu_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
        G_io_apdu_buffer[0] += 2;
    }
    tx = 65;
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    reset_app_context();
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}

unsigned int io_seproxyhal_touch_signMessage_cancel(void) {
    reset_app_context();
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}
