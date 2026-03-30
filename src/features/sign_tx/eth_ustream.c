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

#include "eth_ustream.h"
#include "rlp_utils.h"
#include "common_utils.h"
#include "feature_sign_tx.h"
#include "calldata.h"
#include "tx_ctx.h"  // g_parked_calldata
#include "utils.h"
#include "shared_context.h"  // tmpContent
#include "read.h"            // read_u64_be
#include "network.h"         // get_tx_chain_id
#include "app_mem_utils.h"   // APP_MEM_FREE_AND_NULL

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
    context->store_calldata = store_calldata;
    if (cx_keccak_init_no_throw(context->sha3, 256) != CX_OK) {
        return false;
    }
    return true;
}

static bool read_tx_byte(txContext_t *context, uint8_t *txByte) {
    uint8_t data;
    if (check_cmd_length(context, "read_tx_byte", 1) == false) {
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

bool copy_tx_data(txContext_t *context, uint8_t *out, uint32_t length) {
    if (check_cmd_length(context, "copy_tx_data", length) == false) {
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
    // Feed calldata bytes into the calldata digest hash (includes single-byte fields)
    if (context->calldata_sha3 != NULL) {
        if (cx_hash_no_throw((cx_hash_t *) context->calldata_sha3,
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

static bool process_content(txContext_t *context) {
    // Keep the full length for sanity checks, move to the next field
    if (check_empty_list(context, "RLP_CONTENT") == false) {
        return false;
    }

    context->dataLength = context->currentFieldLength;
    context->currentField++;
    context->processingField = false;
    return true;
}

static bool process_access_list(txContext_t *context) {
    if (check_empty_list(context, "RLP_ACCESS_LIST") == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context, NULL, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool process_auth_list(txContext_t *context) {
    if (check_empty_list(context, "RLP_AUTH_LIST") == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context, NULL, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool process_chain_id(txContext_t *context) {
    if (check_fields(context, "RLP_CHAINID", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context,
                         context->content->chainID.value + context->currentFieldPos,
                         copySize) == false) {
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

static bool process_nonce(txContext_t *context) {
    if (check_fields(context, "RLP_NONCE", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context,
                         context->content->nonce.value + context->currentFieldPos,
                         copySize) == false) {
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

static bool process_start_gas(txContext_t *context) {
    if (check_fields(context, "RLP_STARTGAS", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context,
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

// Alias over `process_start_gas()`.
static bool process_gas_limit(txContext_t *context) {
    return process_start_gas(context);
}

static bool process_gasprice(txContext_t *context) {
    if (check_fields(context, "RLP_GASPRICE", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context,
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

static bool process_value(txContext_t *context) {
    if (check_fields(context, "RLP_VALUE", INT256_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context,
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

static bool process_to(txContext_t *context) {
    if (check_fields(context, "RLP_TO", ADDRESS_LENGTH) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context,
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

static bool process_data(txContext_t *context) {
    uint32_t offset = 0;

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

        if ((context->currentFieldPos == 0) && (copySize >= 4)) {
            // Consider the 4 1st bytes are the selector
            // Store it to be able to check it later against Gating
            memcpy(context->selector_bytes, context->workBuffer, CALLDATA_SELECTOR_SIZE);
        }

        if (context->store_calldata) {
            if (context->currentFieldPos == 0) {
                if (copySize < 4) {
                    PRINTF("Was about to initialize a calldata without a complete selector (%u)!\n",
                           copySize);
                    return false;
                }
                offset = CALLDATA_SELECTOR_SIZE;
                if ((g_parked_calldata = calldata_init(context->currentFieldLength - offset,
                                                       context->workBuffer)) == NULL) {
                    return false;
                }
            }
            if (!calldata_append(g_parked_calldata,
                                 context->workBuffer + offset,
                                 copySize - offset))
                return false;
        }
        if (copy_tx_data(context, NULL, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        PRINTF("incrementing field\n");
        // Finalize calldata digest if we were hashing
        if (context->calldata_sha3 != NULL) {
            if (cx_hash_no_throw((cx_hash_t *) context->calldata_sha3,
                                 CX_LAST,
                                 NULL,
                                 0,
                                 context->calldataDigest,
                                 sizeof(context->calldataDigest)) != CX_OK) {
                return false;
            }
            context->calldataDigestValid = context->content->dataPresent;
            APP_MEM_FREE_AND_NULL((void **) &context->calldata_sha3);
        }
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool process_and_discard(txContext_t *context) {
    if (check_fields(context, "Discarded field", 0) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        if (copy_tx_data(context, NULL, copySize) == false) {
            return false;
        }
    }
    if (context->currentFieldPos == context->currentFieldLength) {
        context->currentField++;
        context->processingField = false;
    }
    return true;
}

static bool process_v(txContext_t *context) {
    if (check_fields(context, "RLP_V", sizeof(context->content->v)) == false) {
        return false;
    }

    if (context->currentFieldPos < context->currentFieldLength) {
        uint32_t copySize =
            MIN(context->commandLength, context->currentFieldLength - context->currentFieldPos);
        // Make sure we do not copy more than the size of v.
        copySize = MIN(copySize, sizeof(context->content->v));
        if (copy_tx_data(context, context->content->v + context->currentFieldPos, copySize) ==
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

static bool process_eip7702_tx(txContext_t *context) {
    bool ret = false;
    switch (context->currentField) {
        case EIP7702_RLP_CONTENT: {
            ret = process_content(context);
            break;
        }
        case EIP7702_RLP_CHAINID: {
            ret = process_chain_id(context);
            break;
        }
        case EIP7702_RLP_NONCE: {
            ret = process_nonce(context);
            break;
        }
        case EIP7702_RLP_MAX_FEE_PER_GAS: {
            ret = process_gasprice(context);
            break;
        }
        case EIP7702_RLP_GASLIMIT: {
            ret = process_gas_limit(context);
            break;
        }
        case EIP7702_RLP_TO: {
            ret = process_to(context);
            break;
        }
        case EIP7702_RLP_VALUE: {
            ret = process_value(context);
            break;
        }
        case EIP7702_RLP_DATA: {
            ret = process_data(context);
            break;
        }
        case EIP7702_RLP_ACCESS_LIST: {
            ret = process_access_list(context);
            break;
        }
        case EIP7702_RLP_AUTH_LIST: {
            ret = process_auth_list(context);
            break;
        }
        case EIP7702_RLP_MAX_PRIORITY_FEE_PER_GAS:
            ret = process_and_discard(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
    }
    return ret;
}

static bool process_eip1559_tx(txContext_t *context) {
    bool ret = false;
    switch (context->currentField) {
        case EIP1559_RLP_CONTENT: {
            ret = process_content(context);
            break;
        }
        case EIP1559_RLP_CHAINID: {
            ret = process_chain_id(context);
            break;
        }
        case EIP1559_RLP_NONCE: {
            ret = process_nonce(context);
            break;
        }
        case EIP1559_RLP_MAX_FEE_PER_GAS: {
            ret = process_gasprice(context);
            break;
        }
        case EIP1559_RLP_GASLIMIT: {
            ret = process_gas_limit(context);
            break;
        }
        case EIP1559_RLP_TO: {
            ret = process_to(context);
            break;
        }
        case EIP1559_RLP_VALUE: {
            ret = process_value(context);
            break;
        }
        case EIP1559_RLP_DATA: {
            ret = process_data(context);
            break;
        }
        case EIP1559_RLP_ACCESS_LIST: {
            ret = process_access_list(context);
            break;
        }
        case EIP1559_RLP_MAX_PRIORITY_FEE_PER_GAS:
            ret = process_and_discard(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
    }
    return ret;
}

static bool process_eip2930_tx(txContext_t *context) {
    bool ret = false;
    switch (context->currentField) {
        case EIP2930_RLP_CONTENT:
            ret = process_content(context);
            break;
        case EIP2930_RLP_CHAINID:
            ret = process_chain_id(context);
            break;
        case EIP2930_RLP_NONCE:
            ret = process_nonce(context);
            break;
        case EIP2930_RLP_GASPRICE:
            ret = process_gasprice(context);
            break;
        case EIP2930_RLP_GASLIMIT:
            ret = process_gas_limit(context);
            break;
        case EIP2930_RLP_TO:
            ret = process_to(context);
            break;
        case EIP2930_RLP_VALUE:
            ret = process_value(context);
            break;
        case EIP2930_RLP_DATA:
            ret = process_data(context);
            break;
        case EIP2930_RLP_ACCESS_LIST:
            ret = process_access_list(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
    }
    return ret;
}

static bool process_legacy_tx(txContext_t *context) {
    bool ret = false;
    switch (context->currentField) {
        case LEGACY_RLP_CONTENT:
            ret = process_content(context);
            break;
        case LEGACY_RLP_NONCE:
            ret = process_nonce(context);
            break;
        case LEGACY_RLP_GASPRICE:
            ret = process_gasprice(context);
            break;
        case LEGACY_RLP_STARTGAS:
            ret = process_start_gas(context);
            break;
        case LEGACY_RLP_TO:
            ret = process_to(context);
            break;
        case LEGACY_RLP_VALUE:
            ret = process_value(context);
            break;
        case LEGACY_RLP_DATA:
            ret = process_data(context);
            break;
        case LEGACY_RLP_R:
        case LEGACY_RLP_S:
            ret = process_and_discard(context);
            break;
        case LEGACY_RLP_V:
            ret = process_v(context);
            break;
        default:
            PRINTF("Invalid RLP decoder context\n");
    }
    return ret;
}

static parserStatus_e parse_rlp(txContext_t *context) {
    bool canDecode = false;
    uint32_t offset;
    while (context->commandLength != 0) {
        bool valid;
        // Feed the RLP buffer until the length can be decoded
        if (read_tx_byte(context, &context->rlpBuffer[context->rlpBufferPos++]) == false) {
            return USTREAM_FAULT;
        }
        if (rlp_can_decode(context->rlpBuffer, context->rlpBufferPos, &valid)) {
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
    if (!rlp_decode_length(context->rlpBuffer,
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

static parserStatus_e process_tx_internal(txContext_t *context) {
    for (;;) {
        customStatus_e customStatus = CUSTOM_NOT_HANDLED;
        // EIP 155 style transaction
        if (PARSING_IS_DONE(context)) {
            if (context->store_calldata) {
                uint8_t to[ADDRESS_LENGTH];
                uint8_t amount[INT256_LENGTH];
                uint64_t chain_id;

                buf_shrink_expand(tmpContent.txContent.destination,
                                  tmpContent.txContent.destinationLength,
                                  to,
                                  sizeof(to));
                buf_shrink_expand(tmpContent.txContent.value.value,
                                  tmpContent.txContent.value.length,
                                  amount,
                                  sizeof(amount));
                chain_id = get_tx_chain_id();

                if (!tx_ctx_init(g_parked_calldata, NULL, to, amount, &chain_id)) {
                    calldata_delete(g_parked_calldata);
                    g_parked_calldata = NULL;
                    return false;
                }
                g_parked_calldata = NULL;
            }
            PRINTF("parsing is done\n");
            return USTREAM_FINISHED;
        }
        if (context->commandLength == 0) {
            PRINTF("Command length done\n");
            return USTREAM_PROCESSING;
        }
        if (!context->processingField) {
            parserStatus_e status = parse_rlp(context);
            if (status != USTREAM_CONTINUE) {
                return status;
            }
        }
        customStatus = custom_processor(context);
        PRINTF("After custom_processor\n");
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
                    if (process_legacy_tx(context) == false) {
                        return USTREAM_FAULT;
                    }
                    break;
                case EIP2930:
                    if (process_eip2930_tx(context) == false) {
                        return USTREAM_FAULT;
                    }
                    break;
                case EIP1559:
                    if (process_eip1559_tx(context) == false) {
                        return USTREAM_FAULT;
                    }
                    break;
                case EIP7702:
                    if (process_eip7702_tx(context) == false) {
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
    return process_tx_internal(context);
}

parserStatus_e continue_tx(txContext_t *context) {
    return process_tx_internal(context);
}
