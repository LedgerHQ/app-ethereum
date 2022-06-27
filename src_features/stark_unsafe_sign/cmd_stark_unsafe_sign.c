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
                               __attribute__((unused)) unsigned int *tx) {
    uint8_t privateKeyData[INT256_LENGTH];
    cx_ecfp_public_key_t publicKey;
    cx_ecfp_private_key_t privateKey;
    uint8_t bip32PathLength;

    // Initial checks
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }

    if ((p1 != 0) || (p2 != 0)) {
        THROW(0x6B00);
    }

    dataBuffer =
        parseBip32(dataBuffer, &dataLength, &bip32PathLength, tmpCtx.transactionContext.bip32Path);

    if (dataLength != 32) {
        THROW(0x6700);
    }

    tmpCtx.transactionContext.pathLength = bip32PathLength;
    memmove(dataContext.starkContext.w2, dataBuffer, 32);
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
