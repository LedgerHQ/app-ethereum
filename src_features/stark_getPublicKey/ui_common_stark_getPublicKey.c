#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "common_ui.h"
#include "feature_stark_getPublicKey.h"

unsigned int io_seproxyhal_touch_stark_pubkey_ok(__attribute__((unused)) const bagl_element_t *e) {
    uint32_t tx = set_result_get_stark_publicKey();
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    reset_app_context();
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}

#endif
