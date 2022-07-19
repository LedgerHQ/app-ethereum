#include "shared_context.h"
#include "apdu_constants.h"
#include "utils.h"
#include "common_ui.h"
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
        THROW(0x6B00);
    }
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }

    workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.messageSigningContext.bip32);

    if ((workBuffer == NULL) || (dataLength < (KECCAK256_HASH_BYTESIZE * 2))) {
        THROW(0x6a80);
    }
    memmove(tmpCtx.messageSigningContext712.domainHash, workBuffer, KECCAK256_HASH_BYTESIZE);
    memmove(tmpCtx.messageSigningContext712.messageHash,
            workBuffer + KECCAK256_HASH_BYTESIZE,
            KECCAK256_HASH_BYTESIZE);

#ifdef NO_CONSENT
    io_seproxyhal_touch_signMessage_ok();
#else   // NO_CONSENT
    ui_sign_712_v0();
#endif  // NO_CONSENT

    *flags |= IO_ASYNCH_REPLY;
}
