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
#include "rlp_utils.h"
#include "common_utils.h"
#include "ledger_assert.h"

#define MAX_INT256  32
#define MAX_ADDRESS 20

static void assert_fields(txContext_t *context, const char *name, uint32_t length) {
    UNUSED(name);  // Just for the case where DEBUG is not enabled
    LEDGER_ASSERT((!context->currentFieldIsList), "Invalid type for %s\n", name);
    if (length > 0) {
        LEDGER_ASSERT((context->currentFieldLength <= length), "Invalid length for %s\n", name);
    }
}

static void assert_empty_list(txContext_t *context, const char *name) {
    UNUSED(name);  // Just for the case where DEBUG is not enabled
    LEDGER_ASSERT((context->currentFieldIsList), "Invalid type for %s\n", name);
}

static void assert_cmd_length(txContext_t *context, const char *name, uint32_t length) {
    UNUSED(name);  // Just for the case where DEBUG is not enabled
    LEDGER_ASSERT((context->commandLength >= length), "%s Underflow\n", name);
}

void initTx(txContext_t *context,
            cx_sha3_t *sha3,
            txContent_t *content,
            ustreamProcess_t customProcessor,
            void *extra) {
    memset(context, 0, sizeof(txContext_t));
    context->sha3 = sha3;
    context->content = content;
    context->customProcessor = customProcessor;
    context->extra = extra;
    context->currentField = RLP_NONE + 1;
    CX_ASSERT(cx_keccak_init_no_throw(context->sha3, 256));
}

static uint8_t readTxByte(txContext_t *context) {
    uint8_t data;
    assert_cmd_length(context, "readTxByte", 1);
    data = *context->workBuffer;
    context->workBuffer++;
    context->commandLength--;
    if (context->processingField) {
        context->currentFieldPos++;
    }
    if (!(context->processingField && context->fieldSingleByte)) {
        CX_ASSERT(cx_hash_no_throw((cx_hash_t *) context->sha3, 0, &data, 1, NULL, 0));
    }
    return data;
}

void copyTxData(txContext_t *context, uint8_t *out, uint32_t length) {
    assert_cmd_length(context, "copyTxData", length);
    if (out != NULL) {
        memmove(out, context->workBuffer, length);
    }
    if (!(context->processingField && context->fieldSingleByte)) {
        CX_ASSERT(
            cx_hash_no_throw((cx_hash_t *) context->sha3, 0, context->workBuffer, length, NULL, 0));
    }
    context->workBuffer += length;
    context->commandLength -= length;
    if (context->processingField) {
        context->currentFieldPos += length;
    }
}

static void processContent(txContext_t *context) {
    // Keep the full length for sanity checks, move to the next field
    assert_empty_list(context, "RLP_CONTENT");
    context->dataLength = context->currentFieldLength;
    context->currentField++;
    context->processingField = false;
}

static void processAccessList(txContext_t *context) {
    assert_empty_list(context, "RLP_ACCESS_LIST");
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
    assert_fields(context, "RLP_CHAINID", MAX_INT256);
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
    assert_fields(context, "RLP_NONCE", MAX_INT256);
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
    assert_fields(context, "RLP_STARTGAS", MAX_INT256);
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
    assert_fields(context, "RLP_GASPRICE", MAX_INT256);
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
    assert_fields(context, "RLP_VALUE", MAX_INT256);
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
    assert_fields(context, "RLP_TO", MAX_ADDRESS);
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
    PRINTF("PROCESS DATA\n");
    assert_fields(context, "RLP_DATA", 0);
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
        PRINTF("incrementing field\n");
        context->currentField++;
        context->processingField = false;
    }
}

static void processAndDiscard(txContext_t *context) {
    assert_fields(context, "Discarded field", 0);
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
    assert_fields(context, "RLP_V", sizeof(context->content->v));

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        // Make sure we do not copy more than the size of v.
        copySize = MIN(copySize, sizeof(context->content->v));
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
        case EIP1559_RLP_MAX_PRIORITY_FEE_PER_GAS:
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
        case EIP2930_RLP_DATA:
            processData(context);
            break;
        case EIP2930_RLP_ACCESS_LIST:
            processAccessList(context);
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
            PRINTF("parsing is done\n");
            return USTREAM_FINISHED;
        }
        // Old style transaction (pre EIP-155). Transactions could just skip `v,r,s` so we needed to
        // cut parsing here. commandLength == 0 could happen in two cases :
        // 1. We are in an old style transaction : just return `USTREAM_FINISHED`.
        // 2. We are at the end of an APDU in a multi-apdu process. This would make us return
        // `USTREAM_FINISHED` preemptively. Case number 2 should NOT happen as it is up to
        // `ledgerjs` to correctly decrease the size of the APDU (`commandLength`) so that this
        // situation doesn't happen.
        if ((context->txType == LEGACY && context->currentField == LEGACY_RLP_V) &&
            (context->commandLength == 0)) {
            context->content->vLength = 0;
            PRINTF("finished\n");
            return USTREAM_FINISHED;
        }
        if (context->commandLength == 0) {
            PRINTF("Command length done\n");
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
            PRINTF("After customprocessor\n");
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
                    PRINTF("Transaction type %d is not supported\n", context->txType);
                    return USTREAM_FAULT;
            }
        }
    }
    PRINTF("end of here\n");
}

parserStatus_e processTx(txContext_t *context, const uint8_t *buffer, uint32_t length) {
    context->workBuffer = buffer;
    context->commandLength = length;
    return processTxInternal(context);
}

parserStatus_e continueTx(txContext_t *context) {
    return processTxInternal(context);
}
