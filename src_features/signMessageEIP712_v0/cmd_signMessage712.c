#include "shared_context.h"
#include "apdu_constants.h"
#include "utils.h"
#include "ui_flow.h"
#include "common_712.h"
#include "ethUtils.h"

void handleSignEIP712Message_v0(uint8_t p1,
                                uint8_t p2,
                                const uint8_t *workBuffer,
                                uint8_t dataLength,
                                unsigned int *flags,
                                unsigned int *tx) {
    (void) tx;
    (void) p2;
    if (p1 != 00) {
        THROW(APDU_RESPONSE_INVALID_P1_P2);
    }
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }

    workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.messageSigningContext.bip32);

    if ((workBuffer == NULL) || (dataLength < (KECCAK256_HASH_BYTESIZE * 2))) {
        THROW(APDU_RESPONSE_INVALID_DATA);
    }
    memmove(tmpCtx.messageSigningContext712.domainHash, workBuffer, KECCAK256_HASH_BYTESIZE);
    memmove(tmpCtx.messageSigningContext712.messageHash,
            workBuffer + KECCAK256_HASH_BYTESIZE,
            KECCAK256_HASH_BYTESIZE);

#ifdef NO_CONSENT
    io_seproxyhal_touch_signMessage_ok(NULL);
#else   // NO_CONSENT
    ux_flow_init(0, ux_sign_712_v0_flow, NULL);
#endif  // NO_CONSENT

    *flags |= IO_ASYNCH_REPLY;
}
