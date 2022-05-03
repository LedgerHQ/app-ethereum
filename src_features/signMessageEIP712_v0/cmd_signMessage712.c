#include "shared_context.h"
#include "apdu_constants.h"
#include "utils.h"
#include "ui_flow.h"

void handleSignEIP712Message_v0(uint8_t p1,
                                uint8_t p2,
                                const uint8_t *workBuffer,
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

    workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.messageSigningContext.bip32);

    if (workBuffer == NULL || dataLength < 32 + 32) {
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
