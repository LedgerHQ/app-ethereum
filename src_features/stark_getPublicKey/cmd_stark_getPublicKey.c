#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "apdu_constants.h"
#include "stark_utils.h"
#include "feature_stark_getPublicKey.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"

void handleStarkwareGetPublicKey(uint8_t p1,
                                 uint8_t p2,
                                 const uint8_t *dataBuffer,
                                 uint8_t dataLength,
                                 unsigned int *flags,
                                 unsigned int *tx) {
    bip32_path_t bip32;
    cx_ecfp_private_key_t privateKey;
    uint8_t privateKeyData[32];

    reset_app_context();

    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        THROW(0x6B00);
    }

    if (p2 != 0) {
        THROW(0x6B00);
    }

    dataBuffer = parseBip32(dataBuffer, &dataLength, &bip32);

    if (dataBuffer == NULL) {
        THROW(0x6a80);
    }

    io_seproxyhal_io_heartbeat();
    starkDerivePrivateKey(bip32.path, bip32.length, privateKeyData);
    cx_ecfp_init_private_key(CX_CURVE_Stark256, privateKeyData, 32, &privateKey);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_generate_pair(CX_CURVE_Stark256, &tmpCtx.publicKeyContext.publicKey, &privateKey, 1);
    explicit_bzero(&privateKey, sizeof(privateKey));
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
    io_seproxyhal_io_heartbeat();
#ifndef NO_CONSENT
    if (p1 == P1_NON_CONFIRM)
#endif  // NO_CONSENT
    {
        *tx = set_result_get_stark_publicKey();
        THROW(0x9000);
    }
#ifndef NO_CONSENT
    else {
        // prepare for a UI based reply
        snprintf(strings.tmp.tmp,
                 sizeof(strings.tmp.tmp),
                 "0x%.*H",
                 32,
                 tmpCtx.publicKeyContext.publicKey.W + 1);
        ui_display_stark_public();

        *flags |= IO_ASYNCH_REPLY;
    }
#endif  // NO_CONSENT
}

#endif
