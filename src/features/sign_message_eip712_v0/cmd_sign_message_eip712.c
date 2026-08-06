#include "apdu_constants.h"
#include "common_utils.h"
#include "common_ui.h"
#include "common_712.h"

uint16_t handle_sign_eip712_message_v0(uint8_t p1, const uint8_t *workBuffer, uint8_t dataLength) {
    if (p1 != 0x00) {
        return SWO_WRONG_P1_P2;
    }
    if (appState != APP_STATE_IDLE) {
        return SWO_COMMAND_NOT_ALLOWED;
    }

    if (!N_storage.dataAllowed) {
        ui_error_blind_signing();
        return SWO_INCORRECT_DATA;
    }
    workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.messageSigningContext.bip32);
    if ((workBuffer == NULL) || (dataLength < (CX_KECCAK_256_SIZE * 2))) {
        return SWO_INCORRECT_DATA;
    }
    memmove(tmpCtx.messageSigningContext712.domainHash, workBuffer, CX_KECCAK_256_SIZE);
    memmove(tmpCtx.messageSigningContext712.messageHash,
            workBuffer + CX_KECCAK_256_SIZE,
            CX_KECCAK_256_SIZE);

    if (!ui_sign_712_v0()) {
        return SWO_INCORRECT_DATA;
    }

    return SWO_NO_RESPONSE;
}
