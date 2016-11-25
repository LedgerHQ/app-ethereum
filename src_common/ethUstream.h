/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
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

#include "os.h"
#include "cx.h"
#include <stdbool.h>

struct txContext_t;

typedef bool (*ustreamProcess_t)(struct txContext_t *context);

typedef enum rlpTxField_e {
    TX_RLP_NONE = 0,
    TX_RLP_CONTENT,
    TX_RLP_NONCE,
    TX_RLP_GASPRICE,
    TX_RLP_STARTGAS,
    TX_RLP_TO,
    TX_RLP_VALUE,
    TX_RLP_DATA,
    TX_RLP_V,
    TX_RLP_R,
    TX_RLP_S,
    TX_RLP_DONE
} rlpTxField_e;

typedef enum parserStatus_e {
    USTREAM_PROCESSING,
    USTREAM_FINISHED,
    USTREAM_FAULT
} parserStatus_e;

typedef struct txInt256_t {
    uint8_t value[32];
    uint8_t length;
} txInt256_t;

typedef struct txContent_t {
    txInt256_t gasprice;
    txInt256_t startgas;
    txInt256_t value;
    uint8_t destination[20];
    uint8_t destinationLength;
    uint8_t v[2];
    uint8_t vLength;
} txContent_t;

typedef struct txContext_t {
    rlpTxField_e currentField;
    cx_sha3_t *sha3;
    uint32_t currentFieldLength;
    uint32_t currentFieldPos;
    bool currentFieldIsList;
    bool processingField;
    bool fieldSingleByte;
    uint32_t dataLength;
    uint8_t rlpBuffer[5];
    uint32_t rlpBufferPos;
    uint8_t *workBuffer;
    uint32_t commandLength;
    ustreamProcess_t customProcessor;
    txContent_t *content;
    void *extra;
} txContext_t;

void initTx(txContext_t *context, cx_sha3_t *sha3, txContent_t *content,
            ustreamProcess_t customProcessor, void *extra);
parserStatus_e processTx(txContext_t *context, uint8_t *buffer,
                         uint32_t length);
void copyTxData(txContext_t *context, uint8_t *out, uint32_t length);
uint8_t readTxByte(txContext_t *context);
