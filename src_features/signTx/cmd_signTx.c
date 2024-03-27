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
        THROW(0x6982);
    }
    if (p1 == P1_FIRST) {
        if (appState != APP_STATE_IDLE) {
            reset_app_context();
        }
        appState = APP_STATE_SIGNING_TX;

        workBuffer = parseBip32(workBuffer, &dataLength, &tmpCtx.transactionContext.bip32);

        if (workBuffer == NULL) {
            THROW(0x6a80);
        }

        tmpContent.txContent.dataPresent = false;
        dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;

        initTx(&txContext, &global_sha3, &tmpContent.txContent, customProcessor, NULL);

        if (dataLength < 1) {
            PRINTF("Invalid data\n");
            THROW(0x6a80);
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
                THROW(0x6501);
            }
        } else {
            txContext.txType = LEGACY;
        }
        PRINTF("TxType: %x\n", txContext.txType);
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
                         (chainConfig->chainId == 888 ? TX_FLAG_TYPE : 0));  // Wanchain exception
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
