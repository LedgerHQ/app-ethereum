#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_flow.h"
#include "feature_signTx.h"

void handleSign(uint8_t p1,
                uint8_t p2,
                uint8_t *workBuffer,
                uint16_t dataLength,
                unsigned int *flags,
                unsigned int *tx) {
    UNUSED(tx);
    parserStatus_e txResult;
    uint32_t i;
    if (p1 == P1_FIRST) {
        if (dataLength < 1) {
            PRINTF("Invalid data\n");
            THROW(0x6a80);
        }
        if (appState != APP_STATE_IDLE) {
            reset_app_context();
        }
        appState = APP_STATE_SIGNING_TX;
        tmpCtx.transactionContext.pathLength = workBuffer[0];
        if ((tmpCtx.transactionContext.pathLength < 0x01) ||
            (tmpCtx.transactionContext.pathLength > MAX_BIP32_PATH)) {
            PRINTF("Invalid path\n");
            THROW(0x6a80);
        }
        workBuffer++;
        dataLength--;
        for (i = 0; i < tmpCtx.transactionContext.pathLength; i++) {
            if (dataLength < 4) {
                PRINTF("Invalid data\n");
                THROW(0x6a80);
            }
            tmpCtx.transactionContext.bip32Path[i] = U4BE(workBuffer, 0);
            workBuffer += 4;
            dataLength -= 4;
        }
        tmpContent.txContent.dataPresent = false;
        dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

        // EIP 2718: TransactionType might be present before the TransactionPayload.
        uint8_t txType = *workBuffer;
        if (txType >= MIN_TX_TYPE && txType <= MAX_TX_TYPE) {
            // Enumerate through all supported txTypes here...
            if (txType == EIP2930 || txType == EIP1559) {
                txContext.txType = txType;
                workBuffer++;
                dataLength--;
            } else {
                PRINTF("Transaction type not supported\n");
                THROW(0x6501);
            }
        } else {
            txContext.txType = LEGACY;
        }
        initTx(&txContext, &global_sha3, &tmpContent.txContent, customProcessor, NULL);
    } else if (p1 != P1_MORE) {
        THROW(0x6B00);
    }
    if (p2 != 0) {
        THROW(0x6B00);
    }
    if ((p1 == P1_MORE) && (appState != APP_STATE_SIGNING_TX)) {
        PRINTF("Signature not initialized\n");
        THROW(0x6985);
    }
    if (txContext.currentField == RLP_NONE) {
        PRINTF("Parser not initialized\n");
        THROW(0x6985);
    }
    txResult = processTx(&txContext,
                         workBuffer,
                         dataLength,
                         (chainConfig->kind == CHAIN_KIND_WANCHAIN ? TX_FLAG_TYPE : 0));
    switch (txResult) {
        case USTREAM_SUSPENDED:
            break;
        case USTREAM_FINISHED:
            break;
        case USTREAM_PROCESSING:
            THROW(0x9000);
        case USTREAM_FAULT:
            THROW(0x6A80);
        default:
            PRINTF("Unexpected parser status\n");
            THROW(0x6A80);
    }

    if (txResult == USTREAM_FINISHED) {
        finalizeParsing(false);
    }

    *flags |= IO_ASYNCH_REPLY;
}
