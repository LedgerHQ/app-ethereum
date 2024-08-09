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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "os.h"
#include "cx.h"
#include "common_utils.h"
#include "tx_content.h"

struct txContext_t;

typedef enum customStatus_e {
    CUSTOM_NOT_HANDLED,
    CUSTOM_HANDLED,
    CUSTOM_SUSPENDED,
    CUSTOM_FAULT
} customStatus_e;

typedef customStatus_e (*ustreamProcess_t)(struct txContext_t *context);

// First variant of every Tx enum.
#define RLP_NONE 0

#define PARSING_IS_DONE(ctx)                                              \
    ((ctx->txType == LEGACY && ctx->currentField == LEGACY_RLP_DONE) ||   \
     (ctx->txType == EIP2930 && ctx->currentField == EIP2930_RLP_DONE) || \
     (ctx->txType == EIP1559 && ctx->currentField == EIP1559_RLP_DONE))

typedef enum rlpLegacyTxField_e {
    LEGACY_RLP_NONE = RLP_NONE,
    LEGACY_RLP_CONTENT,
    LEGACY_RLP_NONCE,
    LEGACY_RLP_GASPRICE,
    LEGACY_RLP_STARTGAS,
    LEGACY_RLP_TO,
    LEGACY_RLP_VALUE,
    LEGACY_RLP_DATA,
    LEGACY_RLP_V,
    LEGACY_RLP_R,
    LEGACY_RLP_S,
    LEGACY_RLP_DONE
} rlpLegacyTxField_e;

typedef enum rlpEIP2930TxField_e {
    EIP2930_RLP_NONE = RLP_NONE,
    EIP2930_RLP_CONTENT,
    EIP2930_RLP_CHAINID,
    EIP2930_RLP_NONCE,
    EIP2930_RLP_GASPRICE,
    EIP2930_RLP_GASLIMIT,
    EIP2930_RLP_TO,
    EIP2930_RLP_VALUE,
    EIP2930_RLP_DATA,
    EIP2930_RLP_ACCESS_LIST,
    EIP2930_RLP_DONE
} rlpEIP2930TxField_e;

typedef enum rlpEIP1559TxField_e {
    EIP1559_RLP_NONE = RLP_NONE,
    EIP1559_RLP_CONTENT,
    EIP1559_RLP_CHAINID,
    EIP1559_RLP_NONCE,
    EIP1559_RLP_MAX_PRIORITY_FEE_PER_GAS,
    EIP1559_RLP_MAX_FEE_PER_GAS,
    EIP1559_RLP_GASLIMIT,
    EIP1559_RLP_TO,
    EIP1559_RLP_VALUE,
    EIP1559_RLP_DATA,
    EIP1559_RLP_ACCESS_LIST,
    EIP1559_RLP_DONE
} rlpEIP1559TxField_e;

#define MIN_TX_TYPE 0x00
#define MAX_TX_TYPE 0x7f

// EIP 2718 TransactionType
// Valid transaction types should be in [0x00, 0x7f]
typedef enum txType_e {
    EIP2930 = 0x01,
    EIP1559 = 0x02,
    LEGACY = 0xc0  // Legacy tx are greater than or equal to 0xc0.
} txType_e;

typedef enum parserStatus_e {
    USTREAM_PROCESSING,  // Parsing is in progress
    USTREAM_SUSPENDED,   // Parsing has been suspended
    USTREAM_FINISHED,    // Parsing is done
    USTREAM_FAULT,       // An error was encountered while parsing
    USTREAM_CONTINUE     // Used internally to signify we can keep on parsing
} parserStatus_e;

typedef struct txContext_t {
    uint8_t currentField;
    cx_sha3_t *sha3;
    uint32_t currentFieldLength;
    uint32_t currentFieldPos;
    bool currentFieldIsList;
    bool processingField;
    bool fieldSingleByte;
    uint32_t dataLength;
    uint8_t rlpBuffer[5];
    uint32_t rlpBufferPos;
    const uint8_t *workBuffer;
    uint32_t commandLength;
    ustreamProcess_t customProcessor;
    txContent_t *content;
    void *extra;
    uint8_t txType;
} txContext_t;

void initTx(txContext_t *context,
            cx_sha3_t *sha3,
            txContent_t *content,
            ustreamProcess_t customProcessor,
            void *extra);
parserStatus_e processTx(txContext_t *context, const uint8_t *buffer, uint32_t length);
parserStatus_e continueTx(txContext_t *context);
void copyTxData(txContext_t *context, uint8_t *out, uint32_t length);
