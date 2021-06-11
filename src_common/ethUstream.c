/*******************************************************************************
 *   Ledger Ethereum App
 *   (c) 2016-2019 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#include <stdint.h>
#include <string.h>

#include "ethUstream.h"
#include "ethUtils.h"
#include "utils.h"

#define MAX_INT256  32
#define MAX_ADDRESS 20
#define MAX_V       4

void initTx(txContext_t *context,
            cx_sha3_t *sha3,
            txContent_t *content,
            ustreamProcess_t customProcessor,
            void *extra) {
    uint8_t save = context->txType;

    memset(context, 0, sizeof(txContext_t));
    context->txType = save;
    context->sha3 = sha3;
    context->content = content;
    context->customProcessor = customProcessor;
    context->extra = extra;
    context->currentField = RLP_NONE + 1;
    cx_keccak_init(context->sha3, 256);
}

uint8_t readTxByte(txContext_t *context) {
    uint8_t data;
    if (context->commandLength < 1) {
        PRINTF("readTxByte Underflow\n");
        THROW(EXCEPTION);
    }
    data = *context->workBuffer;
    context->workBuffer++;
    context->commandLength--;
    if (context->processingField) {
        context->currentFieldPos++;
    }
    if (!(context->processingField && context->fieldSingleByte)) {
        cx_hash((cx_hash_t *) context->sha3, 0, &data, 1, NULL, 0);
    }
    return data;
}

void copyTxData(txContext_t *context, uint8_t *out, uint32_t length) {
    if (context->commandLength < length) {
        PRINTF("copyTxData Underflow\n");
        THROW(EXCEPTION);
    }
    if (out != NULL) {
        memmove(out, context->workBuffer, length);
    }
    if (!(context->processingField && context->fieldSingleByte)) {
        cx_hash((cx_hash_t *) context->sha3, 0, context->workBuffer, length, NULL, 0);
    }
    context->workBuffer += length;
    context->commandLength -= length;
    if (context->processingField) {
        context->currentFieldPos += length;
    }
}

static void processContent(txContext_t *context) {
    // Keep the full length for sanity checks, move to the next field
    if (!context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_CONTENT\n");
        THROW(EXCEPTION);
    }
    context->dataLength = context->currentFieldLength;
    context->currentField++;
    context->processingField = false;
}

static void processAccessList(txContext_t *context) {
    if (!context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_ACCESS_LIST\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, NULL, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
}

static void processType(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_TYPE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_TYPE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, NULL, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
}

static void processChainID(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_CHAINID\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_CHAINID\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->chainID.value, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->chainID.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processNonce(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_NONCE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_NONCE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->nonce.value, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->nonce.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processMaxPriorityFeePerGas(txContext_t *context) {
    PRINTF("PRIORITY\n");
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_MAX_PRIORITY_FEE_PER_GAS\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_MAX_PRIORITY_FEE_PER_GAS\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->maxPriorityFeePerGas.value + context->currentFieldPos, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->maxPriorityFeePerGas.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processStartGas(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_STARTGAS\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_STARTGAS %d\n", context->currentFieldLength);
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->startgas.value + context->currentFieldPos, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->startgas.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

// Alias over `processStartGas()`.
static void processGasLimit(txContext_t *context) {
    processStartGas(context);
}

static void processGasprice(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_GASPRICE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_GASPRICE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->gasprice.value + context->currentFieldPos, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->gasprice.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processValue(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_VALUE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_INT256) {
        PRINTF("Invalid length for RLP_VALUE\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->value.value + context->currentFieldPos, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->value.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processTo(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_TO\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_ADDRESS) {
        PRINTF("Invalid length for RLP_TO\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->destination + context->currentFieldPos, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->destinationLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static void processData(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_DATA\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        // If there is no data, set dataPresent to false.
        if (copySize == 1 && *context->workBuffer == 0x00) {
            context->content->dataPresent = false;
        }
        copyTxData(context, NULL, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
}

static void processAndDiscard(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for Discarded field\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, NULL, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
}

static void processV(txContext_t *context) {
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_V\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldLength > MAX_V) {
        PRINTF("Invalid length for RLP_V\n");
        THROW(EXCEPTION);
    }
    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        copyTxData(context, context->content->v + context->currentFieldPos, copySize);
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->vLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
}

static bool processEIP1559Tx(txContext_t *context) {
    switch (context->currentField) {
        case EIP1559_RLP_CONTENT: {
            processContent(context);
            if ((context->processingFlags & TX_FLAG_TYPE) == 0) {
                context->currentField++;
            }
            break;
        }
        // This gets hit only by Wanchain
        case EIP1559_RLP_TYPE: {
            processType(context);
            break;
        }
        case EIP1559_RLP_CHAINID: {
            processChainID(context);
            break;
        }
        case EIP1559_RLP_NONCE: {
            processNonce(context);
            break;
        }
        case EIP1559_RLP_MAX_PRIORITY_FEE_PER_GAS: {
            processMaxPriorityFeePerGas(context);
            break;
        }
        case EIP1559_RLP_MAX_FEE_PER_GAS: {
            processGasprice(context);
            break;
        }
        case EIP1559_RLP_GASLIMIT: {
            processGasLimit(context);
            break;
        }
        case EIP1559_RLP_TO: {
            processTo(context);
            break;
        }
        case EIP1559_RLP_VALUE: {
            processValue(context);
            break;
        }
        case EIP1559_RLP_DATA: {
            processData(context);
            break;
        }
        case EIP1559_RLP_ACCESS_LIST: {
            processAccessList(context);
            break;
        }
        case EIP1559_RLP_YPARITY: {
            processV(context);
            break;
        }
        case EIP1559_RLP_SENDER_R:
        case EIP1559_RLP_SENDER_S:
            processAndDiscard(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
            return true;
    }
    return false;
}

static bool processEIP2930Tx(txContext_t *context) {
    switch (context->currentField) {
        case EIP2930_RLP_CONTENT:
            processContent(context);
            if ((context->processingFlags & TX_FLAG_TYPE) == 0) {
                context->currentField++;
            }
            break;
        // This gets hit only by Wanchain
        case EIP2930_RLP_TYPE:
            processType(context);
            break;
        case EIP2930_RLP_CHAINID:
            processChainID(context);
            break;
        case EIP2930_RLP_NONCE:
            processNonce(context);
            break;
        case EIP2930_RLP_GASPRICE:
            processGasprice(context);
            break;
        case EIP2930_RLP_GASLIMIT:
            processGasLimit(context);
            break;
        case EIP2930_RLP_TO:
            processTo(context);
            break;
        case EIP2930_RLP_VALUE:
            processValue(context);
            break;
        case EIP2930_RLP_YPARITY:
            processV(context);
            break;
        case EIP2930_RLP_DATA:
            processData(context);
            break;
        case EIP2930_RLP_ACCESS_LIST:
            processAccessList(context);
            break;
        case EIP2930_RLP_SENDER_R:
        case EIP2930_RLP_SENDER_S:
            processAndDiscard(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
            return true;
    }
    return false;
}

static bool processLegacyTx(txContext_t *context) {
    switch (context->currentField) {
        case LEGACY_RLP_CONTENT:
            processContent(context);
            if ((context->processingFlags & TX_FLAG_TYPE) == 0) {
                context->currentField++;
            }
            break;
        // This gets hit only by Wanchain
        case LEGACY_RLP_TYPE:
            processType(context);
            break;
        case LEGACY_RLP_NONCE:
            processNonce(context);
            break;
        case LEGACY_RLP_GASPRICE:
            processGasprice(context);
            break;
        case LEGACY_RLP_STARTGAS:
            processStartGas(context);
            break;
        case LEGACY_RLP_TO:
            processTo(context);
            break;
        case LEGACY_RLP_VALUE:
            processValue(context);
            break;
        case LEGACY_RLP_DATA:
            processData(context);
            break;
        case LEGACY_RLP_R:
        case LEGACY_RLP_S:
            processAndDiscard(context);
            break;
        case LEGACY_RLP_V:
            processV(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
            return true;
    }
    return false;
}

static parserStatus_e parseRLP(txContext_t *context) {
    bool canDecode = false;
    uint32_t offset;
    while (context->commandLength != 0) {
        bool valid;
        // Feed the RLP buffer until the length can be decoded
        context->rlpBuffer[context->rlpBufferPos++] = readTxByte(context);
        if (rlpCanDecode(context->rlpBuffer, context->rlpBufferPos, &valid)) {
            // Can decode now, if valid
            if (!valid) {
                PRINTF("RLP pre-decode error\n");
                return USTREAM_FAULT;
            }
            canDecode = true;
            break;
        }
        // Cannot decode yet
        // Sanity check
        if (context->rlpBufferPos == sizeof(context->rlpBuffer)) {
            PRINTF("RLP pre-decode logic error\n");
            return USTREAM_FAULT;
        }
    }
    if (!canDecode) {
        PRINTF("Can't decode\n");
        return USTREAM_PROCESSING;
    }
    // Ready to process this field
    if (!rlpDecodeLength(context->rlpBuffer,
                         context->rlpBufferPos,
                         &context->currentFieldLength,
                         &offset,
                         &context->currentFieldIsList)) {
        PRINTF("RLP decode error\n");
        return USTREAM_FAULT;
    }
    // Ready to process this field
    if (offset == 0) {
        // Hack for single byte, self encoded
        context->workBuffer--;
        context->commandLength++;
        context->fieldSingleByte = true;
    } else {
        context->fieldSingleByte = false;
    }
    context->currentFieldPos = 0;
    context->rlpBufferPos = 0;
    context->processingField = true;
    return USTREAM_CONTINUE;
}

static parserStatus_e processTxInternal(txContext_t *context) {
    for (;;) {
        customStatus_e customStatus = CUSTOM_NOT_HANDLED;
        // EIP 155 style transaction
        if (PARSING_IS_DONE(context)) {
            return USTREAM_FINISHED;
        }
        // Old style transaction (pre EIP-155). Transations could just skip `v,r,s` so we needed to cut parsing here.
        // commandLength == 0 could happen in two cases :
        // 1. We are in an old style transaction : just return `USTREAM_FINISHED`.
        // 2. We are at the end of an APDU in a multi-apdu process. This would make us return `USTREAM_FINISHED` preemptively.
        // Case number 2 should NOT happen as it is up to `ledgerjs` to correctly decrease the size of the
        // APDU (`commandLength`) so that this situation doesn't happen.
        if ((context->txType == LEGACY && context->currentField == LEGACY_RLP_V) &&
            (context->commandLength == 0)) {
            context->content->vLength = 0;
            return USTREAM_FINISHED;
        }
        if (context->commandLength == 0) {
            return USTREAM_PROCESSING;
        }
        if (!context->processingField) {
            parserStatus_e status = parseRLP(context);
            if (status != USTREAM_CONTINUE) {
                return status;
            }
        }
        if (context->customProcessor != NULL) {
            customStatus = context->customProcessor(context);
            switch (customStatus) {
                case CUSTOM_NOT_HANDLED:
                case CUSTOM_HANDLED:
                    break;
                case CUSTOM_SUSPENDED:
                    return USTREAM_SUSPENDED;
                case CUSTOM_FAULT:
                    PRINTF("Custom processor aborted\n");
                    return USTREAM_FAULT;
                default:
                    PRINTF("Unhandled custom processor status\n");
                    return USTREAM_FAULT;
            }
        }
        if (customStatus == CUSTOM_NOT_HANDLED) {
            PRINTF("Current field: %d\n", context->currentField);
            switch (context->txType) {
                bool fault;
                case LEGACY:
                    fault = processLegacyTx(context);
                    if (fault) {
                        return USTREAM_FAULT;
                    } else {
                        break;
                    }
                case EIP2930:
                    fault = processEIP2930Tx(context);
                    if (fault) {
                        return USTREAM_FAULT;
                    } else {
                        break;
                    }
                case EIP1559:
                    fault = processEIP1559Tx(context);
                    if (fault) {
                        return USTREAM_FAULT;
                    } else {
                        break;
                    }
                default:
                    PRINTF("Transaction type %u is not supported\n", context->txType);
                    return USTREAM_FAULT;
            }
        }
    }
}

parserStatus_e processTx(txContext_t *context,
                         uint8_t *buffer,
                         uint32_t length,
                         uint32_t processingFlags) {
    parserStatus_e result;
    BEGIN_TRY {
        TRY {
            context->workBuffer = buffer;
            context->commandLength = length;
            context->processingFlags = processingFlags;
            result = processTxInternal(context);
        }
        CATCH_OTHER(e) {
            result = USTREAM_FAULT;
        }
        FINALLY {
        }
    }
    END_TRY;
    return result;
}

parserStatus_e continueTx(txContext_t *context) {
    parserStatus_e result;
    BEGIN_TRY {
        TRY {
            result = processTxInternal(context);
        }
        CATCH_OTHER(e) {
            result = USTREAM_FAULT;
        }
        FINALLY {
        }
    }
    END_TRY;
    return result;
}
