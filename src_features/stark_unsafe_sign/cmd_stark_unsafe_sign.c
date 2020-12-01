#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "apdu_constants.h"
#include "stark_utils.h"
#include "ui_flow.h"
#include "ui_callbacks.h"

void handleStarkwareUnsafeSign(uint8_t p1,
                               uint8_t p2,
                               uint8_t *dataBuffer,
                               uint16_t dataLength,
                               unsigned int *flags,
                               unsigned int *tx) {
    uint32_t i;
    uint8_t privateKeyData[32];
    cx_ecfp_public_key_t publicKey;
    cx_ecfp_private_key_t privateKey;
    uint8_t bip32PathLength = *(dataBuffer);
    uint8_t offset = 1;
    // Initial checks
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid path\n");
        THROW(0x6a80);
    }
    if ((p1 != 0) || (p2 != 0)) {
        THROW(0x6B00);
    }

    if (dataLength != 32 + 4 * bip32PathLength + 1) {
        THROW(0x6700);
    }

    tmpCtx.transactionContext.pathLength = bip32PathLength;
    for (i = 0; i < bip32PathLength; i++) {
        tmpCtx.transactionContext.bip32Path[i] = U4BE(dataBuffer, offset);
        PRINTF("Storing path %d %d\n", i, tmpCtx.transactionContext.bip32Path[i]);
        offset += 4;
    }
    memmove(dataContext.starkContext.w2, dataBuffer + offset, 32);
    io_seproxyhal_io_heartbeat();
    starkDerivePrivateKey(tmpCtx.transactionContext.bip32Path, bip32PathLength, privateKeyData);
    cx_ecfp_init_private_key(CX_CURVE_Stark256, privateKeyData, 32, &privateKey);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_generate_pair(CX_CURVE_Stark256, &publicKey, &privateKey, 1);
    explicit_bzero(&privateKey, sizeof(privateKey));
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
    io_seproxyhal_io_heartbeat();
    memmove(dataContext.starkContext.w1, publicKey.W + 1, 32);
    ux_flow_init(0, ux_stark_unsafe_sign_flow, NULL);

    *flags |= IO_ASYNCH_REPLY;
}

#endif
