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

#ifndef _TOKENS_H_
#define _TOKENS_H_

#include <stdint.h>
#include "ethUstream.h"

#define MAX_TICKER_LEN 12  // 10 characters + ' ' + '\0'
#define MAX_ITEMS      2

typedef struct tokenDefinition_t {
    uint8_t address[ADDRESS_LENGTH];  // must be first item
#ifdef HAVE_CONTRACT_NAME_IN_DESCRIPTOR
    uint8_t contractName[ADDRESS_LENGTH];
#endif
    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;
} tokenDefinition_t;

#ifdef HAVE_TOKENS_EXTRA_LIST

#define NUM_TOKENS_EXTRA 8

extern tokenDefinition_t const TOKENS_EXTRA[NUM_TOKENS_EXTRA];

#endif

#endif  // _TOKENS_H_
