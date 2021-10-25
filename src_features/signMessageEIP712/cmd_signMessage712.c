#include "shared_context.h"
#include "apdu_constants.h"
#include "utils.h"
#include "ui_flow.h"

void handleSignEIP712Message(uint8_t p1,
                             uint8_t p2,
                             uint8_t *workBuffer,
                             uint16_t dataLength,
                             unsigned int *flags,
                             unsigned int *tx) {
    uint8_t i;

    UNUSED(tx);
    if ((p1 != 00) || (p2 != 00)) {
        THROW(0x6B00);
    }
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }
    if (dataLength < 1) {
        PRINTF("Invalid data\n");
        THROW(0x6a80);
    }
    tmpCtx.messageSigningContext712.pathLength = workBuffer[0];
    if ((tmpCtx.messageSigningContext712.pathLength < 0x01) ||
        (tmpCtx.messageSigningContext712.pathLength > MAX_BIP32_PATH)) {
        PRINTF("Invalid path\n");
        THROW(0x6a80);
    }
    workBuffer++;
    dataLength--;
    for (i = 0; i < tmpCtx.messageSigningContext712.pathLength; i++) {
        if (dataLength < 4) {
            PRINTF("Invalid data\n");
            THROW(0x6a80);
        }
        tmpCtx.messageSigningContext712.bip32Path[i] = U4BE(workBuffer, 0);
        workBuffer += 4;
        dataLength -= 4;
    }
    if (dataLength < 32 + 32) {
        PRINTF("Invalid data\n");
        THROW(0x6a80);
    }
    memmove(tmpCtx.messageSigningContext712.domainHash, workBuffer, 32);
    memmove(tmpCtx.messageSigningContext712.messageHash, workBuffer + 32, 32);

#ifdef NO_CONSENT
    io_seproxyhal_touch_signMessage_ok(NULL);
#else   // NO_CONSENT
    ux_flow_init(0, ux_sign_712_v0_flow, NULL);
#endif  // NO_CONSENT

    *flags |= IO_ASYNCH_REPLY;
}
