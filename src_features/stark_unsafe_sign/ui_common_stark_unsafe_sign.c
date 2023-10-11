#ifdef HAVE_STARKWARE

#include "os_io_seproxyhal.h"
#include "shared_context.h"
#include "stark_utils.h"
#include "common_ui.h"

unsigned int io_seproxyhal_touch_stark_unsafe_sign_ok(__attribute__((unused))
                                                      const bagl_element_t *e) {
    if (stark_sign_hash(tmpCtx.transactionContext.bip32.path,
                        tmpCtx.transactionContext.bip32.length,
                        dataContext.starkContext.w2,
                        sizeof(dataContext.starkContext.w2),
                        G_io_apdu_buffer) != CX_OK) {
        THROW(0x6F00);
    }

    uint32_t tx = 65;
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
