#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "apdu_constants.h"
#include "stark_utils.h"
#include "feature_stark_getPublicKey.h"
#include "ui_flow.h"

void handleStarkwareGetPublicKey(uint8_t p1,
                                 uint8_t p2,
                                 uint8_t *dataBuffer,
                                 uint16_t dataLength,
                                 unsigned int *flags,
                                 unsigned int *tx) {
    UNUSED(dataLength);
    uint8_t privateKeyData[32];
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint32_t i;
    uint8_t bip32PathLength = *(dataBuffer++);
    cx_ecfp_private_key_t privateKey;
    reset_app_context();
    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid path\n");
        THROW(0x6a80);
    }
    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        THROW(0x6B00);
    }
    if (p2 != 0) {
        THROW(0x6B00);
    }
    for (i = 0; i < bip32PathLength; i++) {
        bip32Path[i] = U4BE(dataBuffer, 0);
        dataBuffer += 4;
    }
    io_seproxyhal_io_heartbeat();
    starkDerivePrivateKey(bip32Path, bip32PathLength, privateKeyData);
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
        ux_flow_init(0, ux_display_stark_public_flow, NULL);

        *flags |= IO_ASYNCH_REPLY;
    }
#endif  // NO_CONSENT
}

#endif
