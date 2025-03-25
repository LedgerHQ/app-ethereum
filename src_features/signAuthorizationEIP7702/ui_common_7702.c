#include "shared_context.h"
#include "common_ui.h"
#include "ui_callbacks.h"
#include "apdu_constants.h"
#include "crypto_helpers.h"

unsigned int auth_7702_ok_cb(void) {
    uint32_t info = 0;
    CX_ASSERT(bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                                  tmpCtx.authSigningContext7702.bip32.path,
                                                  tmpCtx.authSigningContext7702.bip32.length,
                                                  CX_RND_RFC6979 | CX_LAST,
                                                  CX_SHA256,
                                                  tmpCtx.authSigningContext7702.authHash,
                                                  sizeof(tmpCtx.authSigningContext7702.authHash),
                                                  G_io_apdu_buffer + 1,
                                                  G_io_apdu_buffer + 1 + 32,
                                                  &info));
    if (info & CX_ECCINFO_PARITY_ODD) {
        G_io_apdu_buffer[0] = 1;
    } else {
        G_io_apdu_buffer[0] = 0;
    }
    return io_seproxyhal_send_status(APDU_RESPONSE_OK, 65, false, true);
}

unsigned int auth_7702_cancel_cb(void) {
    return io_seproxyhal_send_status(APDU_RESPONSE_CONDITION_NOT_SATISFIED, 0, true, true);
}
