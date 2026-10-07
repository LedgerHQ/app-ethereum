#include "shared_context.h"
#include "apdu_constants.h"
#include "common_utils.h"
#include "get_public_key.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#include "crypto_helpers.h"

uint16_t handle_get_public_key(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *dataBuffer,
                               uint8_t dataLength,
                               unsigned int *tx) {
    bip32_path_t bip32;
    uint64_t chain_id;

    if (!G_called_from_swap) {
        if (appState != APP_STATE_IDLE) {
            return SWO_COMMAND_NOT_ALLOWED;
        }
    }

    if ((p1 != P1_CONFIRM) && (p1 != P1_NON_CONFIRM)) {
        PRINTF("Error: Unexpected P1 (%u)!\n", p1);
        return SWO_WRONG_P1_P2;
    }
    if ((p2 != P2_CHAINCODE) && (p2 != P2_NO_CHAINCODE)) {
        PRINTF("Error: Unexpected P2 (%u)!\n", p2);
        return SWO_WRONG_P1_P2;
    }

    dataBuffer = parseBip32(dataBuffer, &dataLength, &bip32);
    if (dataBuffer == NULL) {
        return SWO_INCORRECT_DATA;
    }

    tmpCtx.publicKeyContext.getChaincode = (p2 == P2_CHAINCODE);
    if (get_public_key_string(
            &bip32,
            tmpCtx.publicKeyContext.publicKey.W,
            tmpCtx.publicKeyContext.address,
            (tmpCtx.publicKeyContext.getChaincode ? tmpCtx.publicKeyContext.chainCode : NULL),
            g_chain_config->chain_id)) {
        return SWO_INCORRECT_DATA;
    }

    if (dataLength >= sizeof(chain_id)) {
        chain_id = u64_from_BE(dataBuffer, sizeof(chain_id));
        dataLength -= sizeof(chain_id);
        dataBuffer += sizeof(chain_id);
        if ((g_chain_config->chain_id != ETHEREUM_MAINNET_CHAINID) &&
            (chain_id != g_chain_config->chain_id)) {
            // clones only accept their own chain ID
            return SWO_INCORRECT_DATA;
        }
    } else {
        chain_id = g_chain_config->chain_id;
    }

    (void) dataBuffer;  // to prevent dead increment warning
    if (dataLength > 0) {
        PRINTF("Error: Leftover unwanted data (%u bytes long)!\n", dataLength);
        return SWO_INCORRECT_DATA;
    }

    if (p1 == P1_NON_CONFIRM) {
        *tx = set_result_get_publicKey();
        return SWO_SUCCESS;
    }
    snprintf(strings.common.toAddress,
             sizeof(strings.common.toAddress),
             "0x%.*s",
             40,
             tmpCtx.publicKeyContext.address);
    appState = APP_STATE_VERIFYING_ADDRESS;
    ui_display_public_key(&chain_id);
    // Return code will be sent after UI approve/cancel
    return SWO_NO_RESPONSE;
}
