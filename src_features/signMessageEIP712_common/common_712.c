#include "shared_context.h"
#include "apdu_constants.h"
#include "os_io_seproxyhal.h"
#include "lib_standard_app/crypto_helpers.h"
#include "ui_callbacks.h"
#include "common_712.h"
#include "ui_callbacks.h"
#include "common_ui.h"

static const uint8_t EIP_712_MAGIC[] = {0x19, 0x01};

unsigned int ui_712_approve_cb(void) {
    uint8_t hash[INT256_LENGTH];
    uint32_t tx = 0;

    io_seproxyhal_io_heartbeat();
    CX_ASSERT(cx_keccak_init_no_throw(&global_sha3, 256));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                               0,
                               (uint8_t *) EIP_712_MAGIC,
                               sizeof(EIP_712_MAGIC),
                               NULL,
                               0));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                               0,
                               tmpCtx.messageSigningContext712.domainHash,
                               sizeof(tmpCtx.messageSigningContext712.domainHash),
                               NULL,
                               0));
    CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                               CX_LAST,
                               tmpCtx.messageSigningContext712.messageHash,
                               sizeof(tmpCtx.messageSigningContext712.messageHash),
                               hash,
                               sizeof(hash)));
    PRINTF("EIP712 Domain hash 0x%.*h\n", 32, tmpCtx.messageSigningContext712.domainHash);
    PRINTF("EIP712 Message hash 0x%.*h\n", 32, tmpCtx.messageSigningContext712.messageHash);

    unsigned int info = 0;
    if (bip32_derive_ecdsa_sign_rs_hash_256(CX_CURVE_256K1,
                                            tmpCtx.messageSigningContext712.bip32.path,
                                            tmpCtx.messageSigningContext712.bip32.length,
                                            CX_RND_RFC6979 | CX_LAST,
                                            CX_SHA256,
                                            hash,
                                            sizeof(hash),
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

unsigned int ui_712_reject_cb(void) {
    reset_app_context();
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0;  // do not redraw the widget
}
