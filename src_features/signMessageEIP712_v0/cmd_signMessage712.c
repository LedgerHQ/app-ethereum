#include "shared_context.h"
#include "apdu_constants.h"
#include "common_utils.h"
#include "common_ui.h"
#include "common_712.h"

uint16_t handleSignEIP712Message_v0(uint8_t p1,
                                    const uint8_t *workBuffer,
                                    uint8_t dataLength,
                                    unsigned int *flags) {
    if (p1 != 0x00) {
        return APDU_RESPONSE_INVALID_P1_P2;
    }
    if (appState != APP_STATE_IDLE) {
        reset_app_context();
    }

    if (!N_storage.dataAllowed) {
        ui_error_blind_signing();
        return APDU_RESPONSE_INVALID_DATA;
    }
    workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.messageSigningContext.bip32);
    if ((workBuffer == NULL) || (dataLength < (KECCAK256_HASH_BYTESIZE * 2))) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    memmove(tmpCtx.messageSigningContext712.domainHash, workBuffer, KECCAK256_HASH_BYTESIZE);
    memmove(tmpCtx.messageSigningContext712.messageHash,
            workBuffer + KECCAK256_HASH_BYTESIZE,
            KECCAK256_HASH_BYTESIZE);

    ui_sign_712_v0();

    *flags |= IO_ASYNCH_REPLY;
    return APDU_NO_RESPONSE;
}
