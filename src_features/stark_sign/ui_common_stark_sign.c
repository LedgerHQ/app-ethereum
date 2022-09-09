#ifdef HAVE_STARKWARE

#include "os_io_seproxyhal.h"
#include "shared_context.h"
#include "stark_utils.h"
#include "common_ui.h"

unsigned int io_seproxyhal_touch_stark_ok(__attribute__((unused)) const bagl_element_t *e) {
    uint8_t privateKeyData[32];
    uint8_t signature[72];
    uint32_t tx = 0;
    io_seproxyhal_io_heartbeat();
    starkDerivePrivateKey(tmpCtx.transactionContext.bip32.path,
                          tmpCtx.transactionContext.bip32.length,
                          privateKeyData);
    io_seproxyhal_io_heartbeat();
    stark_sign(signature,
               privateKeyData,
               dataContext.starkContext.w1,
               dataContext.starkContext.w2,
               dataContext.starkContext.w3,
               (dataContext.starkContext.conditional ? dataContext.starkContext.w4 : NULL));
    G_io_apdu_buffer[0] = 0;
    format_signature_out(signature);
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

#endif  // HAVE_STARKWARE
