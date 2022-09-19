#ifdef HAVE_ETH2

#include "shared_context.h"
#include "feature_getEth2PublicKey.h"
#include "common_ui.h"

unsigned int io_seproxyhal_touch_eth2_address_ok(__attribute__((unused)) const bagl_element_t *e) {
    uint32_t tx = set_result_get_eth2_publicKey();
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    reset_app_context();
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}

#if 0
unsigned int io_seproxyhal_touch_eth2_address_cancel(const bagl_element_t *e) {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    reset_app_context();
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}
#endif

#endif
