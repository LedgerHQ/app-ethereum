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

typedef struct txInt256_t {
    uint8_t value[INT256_LENGTH];
    uint8_t length;
} txInt256_t;

typedef struct txContent_t {
    txInt256_t gasprice;  // Used as MaxFeePerGas when dealing with EIP1559 transactions.
    txInt256_t startgas;  // Also known as `gasLimit`.
    txInt256_t value;
    txInt256_t nonce;
    txInt256_t chainID;
    uint8_t destination[ADDRESS_LENGTH];
    uint8_t destinationLength;
    uint8_t v[8];
    uint8_t vLength;
    bool dataPresent;
} txContent_t;
