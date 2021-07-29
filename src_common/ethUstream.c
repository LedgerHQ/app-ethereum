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
#include "shared_context.h"

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

void decodeThetaTxAddressAndValue(txContext_t *context, uint8_t *rlpP, uint32_t length) {
    uint32_t offset, tmpCurrentFieldLength;
    bool tmpCurrentFieldIsList;
    rlpP += 3;
    memmove(context->content->destination, rlpP, ADDRESS_LENGTH); // decode address
    rlpP += ADDRESS_LENGTH + 1;
    if (*rlpP != 128 && rlpDecodeLength(rlpP, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList)) {//Theta value
        context->content->thetaCoinName = true;
        rlpP += offset;
        memmove(context->content->value.value, rlpP, tmpCurrentFieldLength);                                
        context->content->value.length = tmpCurrentFieldLength;
    } else if (rlpDecodeLength(++rlpP, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList)) { // Tfuel value
        context->content->thetaCoinName = false;
        rlpP += offset;
        memmove(context->content->value.value, rlpP, tmpCurrentFieldLength);
        context->content->value.length = tmpCurrentFieldLength;
    }
    context->content->thetaDecodeSatus = THETATX_FINISHED;
}

void decodeThetaTx(txContext_t *context, uint32_t length) {
    if (context->commandLength < length) {
        PRINTF("copyTxData Underflow\n");
        THROW(EXCEPTION);
    }
    uint32_t offset, tmpCurrentFieldLength;
    bool decoded, tmpCurrentFieldIsList;
    uint8_t *rlpP = context->workBuffer;
    BEGIN_TRY {
        TRY {
            if (context->content->thetaDecodeSatus == THETATX_INIT) {
                decoded = rlpDecodeLength(rlpP, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList);
                rlpP += offset + tmpCurrentFieldLength; // skip chainID
                rlpP++; // skip tx type
                length -= offset + tmpCurrentFieldLength + 1;
                rlpP += 4;
                decoded = rlpDecodeLength(rlpP, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList);
                rlpP += offset;
                memmove(context->content->startgas.value, rlpP, tmpCurrentFieldLength); // decode gas
                context->content->startgas.length = tmpCurrentFieldLength;
                context->content->gasprice.value[0] = 1;    // set gas price to 1
                context->content->gasprice.length = 1;
                rlpP += tmpCurrentFieldLength;
                length -= offset + tmpCurrentFieldLength + 4;
                decoded = rlpDecodeLength(rlpP, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList);
                rlpP += offset + tmpCurrentFieldLength; // skip
                length -= offset + tmpCurrentFieldLength;
                decoded = rlpDecodeLength(rlpP, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList);
                if (tmpCurrentFieldLength + 1 > length) {   // Not enought bytes to decode address/value
                    context->content->thetaDecodeSatus = THETATX_PROCESSING;
                    context->content->thetaBufferToRead = tmpCurrentFieldLength - length + 1;
                    memmove(context->content->thetaBuffer, rlpP, length);
                    context->content->thetaBufferLength = length;
                } else {
                    decodeThetaTxAddressAndValue(context, rlpP, tmpCurrentFieldLength);
                }
            } else if (context->content->thetaDecodeSatus == THETATX_PROCESSING) { // decode with Theta buffer
                if (length < context->content->thetaBufferToRead) {
                    context->content->thetaDecodeSatus = THETATX_FAULT;
                    PRINTF("Decode ThetaTx failed, invalid buffer");
                    return;
                }
                memmove(context->content->thetaBuffer + context->content->thetaBufferLength, rlpP, context->content->thetaBufferToRead);
                decodeThetaTxAddressAndValue(context, context->content->thetaBuffer, context->content->thetaBufferLength + context->content->thetaBufferToRead);
            }
        }
        CATCH_OTHER(e) {
            context->content->thetaDecodeSatus = THETATX_FAULT;
            PRINTF("Decode ThetaTx failed ");
        }
        FINALLY {
        }
    }
    END_TRY;
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
    if (chainConfig->kind == CHAIN_KIND_THETA) { // reset Theta decoding status
        context->content->thetaDecodeSatus = THETATX_INIT;
    }
}

static void processAccessList(txContext_t *context) {
    if (!context->currentFieldIsList) {
        PRINTF("Invalid type for RLP_DATA\n");
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
        // if (chainConfig->kind == CHAIN_KIND_THETA) {
        if (allzeroes(context->content->destination, sizeof(context->content->destination)) && allzeroes(&context->content->value, sizeof(context->content->value))
            && allzeroes(&context->content->gasprice, sizeof(context->content->gasprice)) || chainConfig->kind == CHAIN_KIND_THETA) { //Theta Tx
            chainConfig->kind = CHAIN_KIND_THETA;
            decodeThetaTx(context, copySize);
        }
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

static bool processEIP2930Tx(txContext_t *context) {
    switch (context->currentField) {
        case EIP2930_RLP_CONTENT:
            processContent(context);
            if ((context->processingFlags & TX_FLAG_TYPE) == 0) {
                context->currentField++;
            }
            break;
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
        case EIP2930_RLP_ACCESS_LIST:
            processAccessList(context);
            break;
        case EIP2930_RLP_DATA:
        case EIP2930_RLP_SENDER_R:
        case EIP2930_RLP_SENDER_S:
            processData(context);
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
        case LEGACY_RLP_R:
        case LEGACY_RLP_S:
            processData(context);
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
                         &context->currentFieldLength,
                         &offset,
                         &context->currentFieldIsList)) {
        PRINTF("RLP decode error\n");
        return USTREAM_FAULT;
    }
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
        // Old style transaction
        if (((context->txType == LEGACY && context->currentField == LEGACY_RLP_V) ||
             (context->txType == EIP2930 && context->currentField == EIP2930_RLP_YPARITY)) &&
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
            PRINTF("Current field: %u\n", context->currentField);
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
