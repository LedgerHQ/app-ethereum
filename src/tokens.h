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

typedef struct tokenDefinition_t {
    uint8_t address[20];
    uint8_t ticker[10];
    uint8_t decimals;
} tokenDefinition_t;

#ifdef HAVE_TOKENS_LIST

#define NUM_TOKENS_AKROMA 0
#define NUM_TOKENS_ELLAISM 1
#define NUM_TOKENS_ETHEREUM 1102
#define NUM_TOKENS_ETHEREUM_CLASSIC 4
#define NUM_TOKENS_ETHERSOCIAL 0
#define NUM_TOKENS_ETHER1 0
#define NUM_TOKENS_PIRL 0
#define NUM_TOKENS_POA 0
#define NUM_TOKENS_RSK 0
#define NUM_TOKENS_UBIQ 6
#define NUM_TOKENS_EXPANSE 0
#define NUM_TOKENS_WANCHAIN 0
#define NUM_TOKENS_KUSD 0
#define NUM_TOKENS_MUSICOIN 0
#define NUM_TOKENS_CALLISTO 0
#define NUM_TOKENS_ETHERGEM 0
#define NUM_TOKENS_ATHEIOS 0
#define NUM_TOKENS_GOCHAIN 0
#define NUM_TOKENS_MIX 0
#define NUM_TOKENS_REOSC 0
#define NUM_TOKENS_HPB 0
#define NUM_TOKENS_TOMOCHAIN 0
#define NUM_TOKENS_TOBALABA 0
#define NUM_TOKENS_VOLTA 0
#define NUM_TOKENS_EWC 0

extern tokenDefinition_t const TOKENS_AKROMA[NUM_TOKENS_AKROMA];
extern tokenDefinition_t const TOKENS_ELLAISM[NUM_TOKENS_ELLAISM];
extern tokenDefinition_t const TOKENS_ETHEREUM[NUM_TOKENS_ETHEREUM];
extern tokenDefinition_t const TOKENS_ETHEREUM_CLASSIC[NUM_TOKENS_ETHEREUM_CLASSIC];
extern tokenDefinition_t const TOKENS_ETHERSOCIAL[NUM_TOKENS_ETHERSOCIAL];
extern tokenDefinition_t const TOKENS_ETHER1[NUM_TOKENS_ETHER1];
extern tokenDefinition_t const TOKENS_PIRL[NUM_TOKENS_PIRL];
extern tokenDefinition_t const TOKENS_POA[NUM_TOKENS_POA];
extern tokenDefinition_t const TOKENS_RSK[NUM_TOKENS_RSK];
extern tokenDefinition_t const TOKENS_UBIQ[NUM_TOKENS_UBIQ];
extern tokenDefinition_t const TOKENS_EXPANSE[NUM_TOKENS_EXPANSE];
extern tokenDefinition_t const TOKENS_WANCHAIN[NUM_TOKENS_WANCHAIN];
extern tokenDefinition_t const TOKENS_KUSD[NUM_TOKENS_KUSD];
extern tokenDefinition_t const TOKENS_MUSICOIN[NUM_TOKENS_MUSICOIN];
extern tokenDefinition_t const TOKENS_CALLISTO[NUM_TOKENS_CALLISTO];
extern tokenDefinition_t const TOKENS_ETHERGEM[NUM_TOKENS_ETHERGEM];
extern tokenDefinition_t const TOKENS_ATHEIOS[NUM_TOKENS_ATHEIOS];
extern tokenDefinition_t const TOKENS_GOCHAIN[NUM_TOKENS_GOCHAIN];
extern tokenDefinition_t const TOKENS_MIX[NUM_TOKENS_MIX];
extern tokenDefinition_t const TOKENS_REOSC[NUM_TOKENS_REOSC];
extern tokenDefinition_t const TOKENS_HPB[NUM_TOKENS_HPB];
extern tokenDefinition_t const TOKENS_TOMOCHAIN[NUM_TOKENS_TOMOCHAIN];
extern tokenDefinition_t const TOKENS_TOBALABA[NUM_TOKENS_TOBALABA];
extern tokenDefinition_t const TOKENS_VOLTA[NUM_TOKENS_VOLTA];
extern tokenDefinition_t const TOKENS_EWC[NUM_TOKENS_EWC];

#endif

#endif /* _TOKENS_H_ */
