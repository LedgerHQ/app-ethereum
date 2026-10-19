#include "os_io_seproxyhal.h"
#include "apdu_constants.h"
#include "crypto_helpers.h"
#include "common_ui.h"

bool io_seproxyhal_touch_signMessage_ok(void) {
    if (appState != APP_STATE_SIGNING_MESSAGE) {
        io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, false);
        return false;
    }
    unsigned int info = 0;
    // The host-provided path is only checked by the OS here, after the review
    if (bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                            tmpCtx.messageSigningContext.bip32.path,
                                            tmpCtx.messageSigningContext.bip32.length,
                                            CX_RND_RFC6979 | CX_LAST,
                                            CX_SHA256,
                                            tmpCtx.messageSigningContext.hash,
                                            sizeof(tmpCtx.messageSigningContext.hash),
                                            G_io_tx_buffer + 1,
                                            G_io_tx_buffer + 1 + INT256_LENGTH,
                                            &info) != CX_OK) {
        explicit_bzero(G_io_tx_buffer, ECDSA_SIGNATURE_LENGTH);
        io_seproxyhal_send_status(SWO_INCORRECT_DATA, 0, true, false);
        return false;
    }
    G_io_tx_buffer[0] = ETHEREUM_SIGNATURE_V_BASE;
    if (info & CX_ECCINFO_PARITY_ODD) {
        G_io_tx_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
        G_io_tx_buffer[0] += 2;
    }
    io_seproxyhal_send_status(SWO_SUCCESS, ECDSA_SIGNATURE_LENGTH, true, false);
    return true;
}

unsigned int io_seproxyhal_touch_signMessage_cancel(void) {
    return io_seproxyhal_send_status(SWO_CONDITIONS_NOT_SATISFIED, 0, true, false);
}
