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
#include "feature_signTx.h"
#ifdef HAVE_GENERIC_TX_PARSER
#include "calldata.h"
#endif

static bool check_fields(txContext_t *context, const char *name, uint32_t length) {
    UNUSED(name);  // Just for the case where DEBUG is not enabled
    if (context->currentFieldIsList) {
        PRINTF("Invalid type for %s\n", name);
        return false;
    }
    if ((length > 0) && (context->currentFieldLength > length)) {
        PRINTF("Invalid length for %s\n", name);
        return false;
    }
    return true;
}

static bool check_empty_list(txContext_t *context, const char *name) {
    UNUSED(name);  // Just for the case where DEBUG is not enabled
    if (!context->currentFieldIsList) {
        PRINTF("Invalid type for %s\n", name);
        return false;
    }
    return true;
}

static bool check_cmd_length(txContext_t *context, const char *name, uint32_t length) {
    UNUSED(name);  // Just for the case where DEBUG is not enabled
    if (context->commandLength < length) {
        PRINTF("%s Underflow\n", name);
        return false;
    }
    return true;
}

bool init_tx(txContext_t *context, cx_sha3_t *sha3, txContent_t *content, bool store_calldata) {
    explicit_bzero(context, sizeof(*context));
    context->sha3 = sha3;
    context->content = content;
    context->currentField = RLP_NONE + 1;
#ifdef HAVE_GENERIC_TX_PARSER
    context->store_calldata = store_calldata;
#else
    UNUSED(store_calldata);
#endif
    if (cx_keccak_init_no_throw(context->sha3, 256) != CX_OK) {
        return false;
    }
    return true;
}

static bool readTxByte(txContext_t *context, uint8_t *txByte) {
    uint8_t data;
    if (check_cmd_length(context, "readTxByte", 1) == false) {
        return false;
    }
    data = *context->workBuffer;
    context->workBuffer++;
    context->commandLength--;
    if (context->processingField) {
        context->currentFieldPos++;
    }
    if (!(context->processingField && context->fieldSingleByte)) {
        if (cx_hash_no_throw((cx_hash_t *) context->sha3, 0, &data, 1, NULL, 0) != CX_OK) {
            return false;
        }
    }
    *txByte = data;
    return true;
}

bool copyTxData(txContext_t *context, uint8_t *out, uint32_t length) {
    if (check_cmd_length(context, "copyTxData", length) == false) {
        return false;
    }
    if (out != NULL) {
        memmove(out, context->workBuffer, length);
    }
    if (!(context->processingField && context->fieldSingleByte)) {
        if (cx_hash_no_throw((cx_hash_t *) context->sha3,
                             0,
                             context->workBuffer,
                             length,
                             NULL,
                             0) != CX_OK) {
            return false;
        }
    }
    context->workBuffer += length;
    context->commandLength -= length;
    if (context->processingField) {
        context->currentFieldPos += length;
    }
    return true;
}

static bool processContent(txContext_t *context) {
    // Keep the full length for sanity checks, move to the next field
    if (check_empty_list(context, "RLP_CONTENT") == false) {
        return false;
    }

    context->dataLength = context->currentFieldLength;
    context->currentField++;
    context->processingField = false;
    return true;
}

static bool processAccessList(txContext_t *context) {
    if (check_empty_list(context, "RLP_ACCESS_LIST") == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, NULL, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processChainID(txContext_t *context) {
    if (check_fields(context, "RLP_CHAINID", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->chainID.value, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->chainID.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processNonce(txContext_t *context) {
    if (check_fields(context, "RLP_NONCE", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, context->content->nonce.value, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->nonce.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processStartGas(txContext_t *context) {
    if (check_fields(context, "RLP_STARTGAS", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context,
                       context->content->startgas.value + context->currentFieldPos,
                       copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->startgas.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

// Alias over `processStartGas()`.
static bool processGasLimit(txContext_t *context) {
    return processStartGas(context);
}

static bool processGasprice(txContext_t *context) {
    if (check_fields(context, "RLP_GASPRICE", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context,
                       context->content->gasprice.value + context->currentFieldPos,
                       copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->gasprice.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processValue(txContext_t *context) {
    if (check_fields(context, "RLP_VALUE", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context,
                       context->content->value.value + context->currentFieldPos,
                       copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->value.length = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processTo(txContext_t *context) {
    if (check_fields(context, "RLP_TO", ADDRESS_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context,
                       context->content->destination + context->currentFieldPos,
                       copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->destinationLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processData(txContext_t *context) {
    PRINTF("PROCESS DATA\n");
    if (check_fields(context, "RLP_DATA", 0) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        // If there is no data, set dataPresent to false.
        if (copySize == 1 && *context->workBuffer == 0x00) {
            context->content->dataPresent = false;
        }
#ifdef HAVE_GENERIC_TX_PARSER
        if (context->store_calldata) {
            if (context->currentFieldPos == 0) {
                if (!calldata_init(context->currentFieldLength)) {
                    return false;
                }
            }
            calldata_append(context->workBuffer, copySize);
        }
#endif
        if (copyTxData(context, NULL, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        PRINTF("incrementing field\n");
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processAndDiscard(txContext_t *context) {
    if (check_fields(context, "Discarded field", 0) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copyTxData(context, NULL, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processV(txContext_t *context) {
    if (check_fields(context, "RLP_V", sizeof(context->content->v)) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        // Make sure we do not copy more than the size of v.
        copySize = MIN(copySize, sizeof(context->content->v));
        if (copyTxData(context, context->content->v + context->currentFieldPos, copySize) ==
            false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->content->vLength = context->currentFieldLength;
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool processEIP1559Tx(txContext_t *context) {
    bool ret = false;
    switch (context->currentField) {
        case EIP1559_RLP_CONTENT: {
            ret = processContent(context);
            break;
        }
        case EIP1559_RLP_CHAINID: {
            ret = processChainID(context);
            break;
        }
        case EIP1559_RLP_NONCE: {
            ret = processNonce(context);
            break;
        }
        case EIP1559_RLP_MAX_FEE_PER_GAS: {
            ret = processGasprice(context);
            break;
        }
        case EIP1559_RLP_GASLIMIT: {
            ret = processGasLimit(context);
            break;
        }
        case EIP1559_RLP_TO: {
            ret = processTo(context);
            break;
        }
        case EIP1559_RLP_VALUE: {
            ret = processValue(context);
            break;
        }
        case EIP1559_RLP_DATA: {
            ret = processData(context);
            break;
        }
        case EIP1559_RLP_ACCESS_LIST: {
            ret = processAccessList(context);
            break;
        }
        case EIP1559_RLP_MAX_PRIORITY_FEE_PER_GAS:
            ret = processAndDiscard(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
    }
    return ret;
}

static bool processEIP2930Tx(txContext_t *context) {
    bool ret = false;
    switch (context->currentField) {
        case EIP2930_RLP_CONTENT:
            ret = processContent(context);
            break;
        case EIP2930_RLP_CHAINID:
            ret = processChainID(context);
            break;
        case EIP2930_RLP_NONCE:
            ret = processNonce(context);
            break;
        case EIP2930_RLP_GASPRICE:
            ret = processGasprice(context);
            break;
        case EIP2930_RLP_GASLIMIT:
            ret = processGasLimit(context);
            break;
        case EIP2930_RLP_TO:
            ret = processTo(context);
            break;
        case EIP2930_RLP_VALUE:
            ret = processValue(context);
            break;
        case EIP2930_RLP_DATA:
            ret = processData(context);
            break;
        case EIP2930_RLP_ACCESS_LIST:
            ret = processAccessList(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
    }
    return ret;
}

static bool processLegacyTx(txContext_t *context) {
    bool ret = false;
    switch (context->currentField) {
        case LEGACY_RLP_CONTENT:
            ret = processContent(context);
            break;
        case LEGACY_RLP_NONCE:
            ret = processNonce(context);
            break;
        case LEGACY_RLP_GASPRICE:
            ret = processGasprice(context);
            break;
        case LEGACY_RLP_STARTGAS:
            ret = processStartGas(context);
            break;
        case LEGACY_RLP_TO:
            ret = processTo(context);
            break;
        case LEGACY_RLP_VALUE:
            ret = processValue(context);
            break;
        case LEGACY_RLP_DATA:
            ret = processData(context);
            break;
        case LEGACY_RLP_R:
        case LEGACY_RLP_S:
            ret = processAndDiscard(context);
            break;
        case LEGACY_RLP_V:
            ret = processV(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
    }
    return ret;
}

static parserStatus_e parseRLP(txContext_t *context) {
    bool canDecode = false;
    uint32_t offset;
    while (context->commandLength != 0) {
        bool valid;
        // Feed the RLP buffer until the length can be decoded
        if (readTxByte(context, &context->rlpBuffer[context->rlpBufferPos++]) == false) {
            return USTREAM_FAULT;
        }
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
        customStatus = customProcessor(context);
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
        if (customStatus == CUSTOM_NOT_HANDLED) {
            PRINTF("Current field: %d\n", context->currentField);
            switch (context->txType) {
                case LEGACY:
                    if (processLegacyTx(context) == false) {
                        return USTREAM_FAULT;
                    }
                    break;
                case EIP2930:
                    if (processEIP2930Tx(context) == false) {
                        return USTREAM_FAULT;
                    }
                    break;
                case EIP1559:
                    if (processEIP1559Tx(context) == false) {
                        return USTREAM_FAULT;
                    }
                    break;
                default:
                    PRINTF("Transaction type %d is not supported\n", context->txType);
                    return USTREAM_FAULT;
            }
        }
    }
    PRINTF("end of here\n");
}

parserStatus_e process_tx(txContext_t *context, const uint8_t *payload, size_t length) {
    context->workBuffer = payload;
    context->commandLength = length;
    return processTxInternal(context);
}

parserStatus_e continueTx(txContext_t *context) {
    return processTxInternal(context);
}
