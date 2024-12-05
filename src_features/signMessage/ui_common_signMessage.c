#include "os_io_seproxyhal.h"
#include "apdu_constants.h"
#include "crypto_helpers.h"
#include "common_ui.h"

unsigned int io_seproxyhal_touch_signMessage_ok(void) {
    unsigned int info = 0;
    CX_ASSERT(bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                                  tmpCtx.messageSigningContext.bip32.path,
                                                  tmpCtx.messageSigningContext.bip32.length,
                                                  CX_RND_RFC6979 | CX_LAST,
                                                  CX_SHA256,
                                                  tmpCtx.messageSigningContext.hash,
                                                  sizeof(tmpCtx.messageSigningContext.hash),
                                                  G_io_apdu_buffer + 1,
                                                  G_io_apdu_buffer + 1 + 32,
                                                  &info));
    G_io_apdu_buffer[0] = 27;
    if (info & CX_ECCINFO_PARITY_ODD) {
        G_io_apdu_buffer[0]++;
    }
    if (info & CX_ECCINFO_xGTn) {
        G_io_apdu_buffer[0] += 2;
    }
    return io_seproxyhal_send_status(APDU_RESPONSE_OK, 65, true, false);
}

unsigned int io_seproxyhal_touch_signMessage_cancel(void) {
    return io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, false);
}
