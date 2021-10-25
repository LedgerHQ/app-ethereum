#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "stark_utils.h"
#include "ui_callbacks.h"

unsigned int io_seproxyhal_touch_stark_unsafe_sign_ok(__attribute__((unused))
                                                      const bagl_element_t *e) {
    cx_ecfp_private_key_t privateKey;
    uint8_t privateKeyData[INT256_LENGTH];
    uint8_t signature[72];
    unsigned int info = 0;
    uint32_t tx = 0;
    io_seproxyhal_io_heartbeat();
    starkDerivePrivateKey(tmpCtx.transactionContext.bip32Path,
                          tmpCtx.transactionContext.pathLength,
                          privateKeyData);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_init_private_key(CX_CURVE_Stark256, privateKeyData, 32, &privateKey);
    cx_ecdsa_sign(&privateKey,
                  CX_RND_RFC6979 | CX_LAST,
                  CX_SHA256,
                  dataContext.starkContext.w2,
                  sizeof(dataContext.starkContext.w2),
                  signature,
                  sizeof(signature),
                  &info);
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

#endif
