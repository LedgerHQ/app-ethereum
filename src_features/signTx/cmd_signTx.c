#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_signTx.h"
#include "eth_plugin_interface.h"

uint16_t handleSign(uint8_t p1,
                    uint8_t p2,
                    const uint8_t *workBuffer,
                    uint8_t dataLength,
                    unsigned int *flags) {
    parserStatus_e txResult;
    uint16_t sw = APDU_NO_RESPONSE;
    cx_err_t error = CX_INTERNAL_ERROR;

    if (os_global_pin_is_validated() != BOLOS_UX_OK) {
        PRINTF("Device is PIN-locked");
        return APDU_RESPONSE_SECURITY_NOT_SATISFIED;
    }
    if (p1 == P1_FIRST) {
        if (appState != APP_STATE_IDLE) {
            reset_app_context();
        }
        appState = APP_STATE_SIGNING_TX;

        workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.transactionContext.bip32);
        if (workBuffer == NULL) {
            return APDU_RESPONSE_INVALID_DATA;
        }

        tmpContent.txContent.dataPresent = false;
        dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

        if (initTx(&txContext, &global_sha3, &tmpContent.txContent, customProcessor, NULL) ==
            false) {
            return APDU_RESPONSE_INVALID_DATA;
        }
        if (dataLength < 1) {
            PRINTF("Invalid data\n");
            return APDU_RESPONSE_INVALID_DATA;
        }

        // EIP 2718: TransactionType might be present before the TransactionPayload.
        uint8_t txType = *workBuffer;
        if (txType >= MIN_TX_TYPE && txType <= MAX_TX_TYPE) {
            // Enumerate through all supported txTypes here...
            if (txType == EIP2930 || txType == EIP1559) {
                error = cx_hash_no_throw((cx_hash_t *) &global_sha3, 0, workBuffer, 1, NULL, 0);
                if (error != CX_OK) {
                    return error;
                }
                txContext.txType = txType;
                workBuffer++;
                dataLength--;
            } else {
                PRINTF("Transaction type %d not supported\n", txType);
                return APDU_RESPONSE_TX_TYPE_NOT_SUPPORTED;
            }
        } else {
            txContext.txType = LEGACY;
        }
        PRINTF("TxType: %x\n", txContext.txType);
    } else if (p1 != P1_MORE) {
        return APDU_RESPONSE_INVALID_P1_P2;
    }
    if (p2 != 0) {
        return APDU_RESPONSE_INVALID_P1_P2;
    }
    if ((p1 == P1_MORE) && (appState != APP_STATE_SIGNING_TX)) {
        PRINTF("Signature not initialized\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    if (txContext.currentField == RLP_NONE) {
        PRINTF("Parser not initialized\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    txResult = processTx(&txContext, workBuffer, dataLength);
    switch (txResult) {
        case USTREAM_SUSPENDED:
            break;
        case USTREAM_FINISHED:
            sw = finalizeParsing();
            break;
        case USTREAM_PROCESSING:
            return APDU_RESPONSE_OK;
        case USTREAM_FAULT:
            if (G_called_from_swap) {
                // We have encountered an error while trying to sign a SWAP type transaction
                // Return dedicated error code and flag an early exit back to Exchange
                G_swap_response_ready = true;
                return APDU_RESPONSE_MODE_CHECK_FAILED;
            } else {
                return APDU_RESPONSE_INVALID_DATA;
            }
        default:
            PRINTF("Unexpected parser status\n");
            return APDU_RESPONSE_INVALID_DATA;
    }

    *flags |= IO_ASYNCH_REPLY;
    // Return code will be sent after UI approve/cancel
    return sw;
}
