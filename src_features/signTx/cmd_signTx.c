#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_signTx.h"
#include "eth_plugin_interface.h"

void handleSign(uint8_t p1,
                uint8_t p2,
                const uint8_t *workBuffer,
                uint8_t dataLength,
                unsigned int *flags,
                unsigned int *tx) {
    UNUSED(tx);
    parserStatus_e txResult;

    if (os_global_pin_is_validated() != BOLOS_UX_OK) {
        PRINTF("Device is PIN-locked");
        THROW(APDU_RESPONSE_SECURITY_NOT_SATISFIED);
    }
    if (p1 == P1_FIRST) {
        if (appState != APP_STATE_IDLE) {
            reset_app_context();
        }
        appState = APP_STATE_SIGNING_TX;

        workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.transactionContext.bip32);

        if (workBuffer == NULL) {
            THROW(APDU_RESPONSE_INVALID_DATA);
        }

        tmpContent.txContent.dataPresent = false;
        dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

        initTx(&txContext, &global_sha3, &tmpContent.txContent, customProcessor, NULL);

        if (dataLength < 1) {
            PRINTF("Invalid data\n");
            THROW(APDU_RESPONSE_INVALID_DATA);
        }

        // EIP 2718: TransactionType might be present before the TransactionPayload.
        uint8_t txType = *workBuffer;
        if (txType >= MIN_TX_TYPE && txType <= MAX_TX_TYPE) {
            // Enumerate through all supported txTypes here...
            if (txType == EIP2930 || txType == EIP1559) {
                CX_ASSERT(cx_hash_no_throw((cx_hash_t *) &global_sha3, 0, workBuffer, 1, NULL, 0));
                txContext.txType = txType;
                workBuffer++;
                dataLength--;
            } else {
                PRINTF("Transaction type %d not supported\n", txType);
                THROW(APDU_RESPONSE_TX_TYPE_NOT_SUPPORTED);
            }
        } else {
            txContext.txType = LEGACY;
        }
        PRINTF("TxType: %x\n", txContext.txType);
    } else if (p1 != P1_MORE) {
        THROW(APDU_RESPONSE_INVALID_P1_P2);
    }
    if (p2 != 0) {
        THROW(APDU_RESPONSE_INVALID_P1_P2);
    }
    if ((p1 == P1_MORE) && (appState != APP_STATE_SIGNING_TX)) {
        PRINTF("Signature not initialized\n");
        THROW(APDU_RESPONSE_CONDITION_NOT_SATISFIED);
    }
    if (txContext.currentField == RLP_NONE) {
        PRINTF("Parser not initialized\n");
        THROW(APDU_RESPONSE_CONDITION_NOT_SATISFIED);
    }
    txResult = processTx(&txContext, workBuffer, dataLength);
    switch (txResult) {
        case USTREAM_SUSPENDED:
            break;
        case USTREAM_FINISHED:
            break;
        case USTREAM_PROCESSING:
            THROW(APDU_RESPONSE_OK);
        case USTREAM_FAULT:
            THROW(APDU_RESPONSE_INVALID_DATA);
        default:
            PRINTF("Unexpected parser status\n");
            THROW(APDU_RESPONSE_INVALID_DATA);
    }

    if (txResult == USTREAM_FINISHED) {
        finalizeParsing();
    }

    *flags |= IO_ASYNCH_REPLY;
}
