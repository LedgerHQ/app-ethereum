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

#define MAX_TICKER_LEN 11  // 10 characters + '\0'
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

#ifndef HAVE_TOKENS_LIST

#if defined(HAVE_CAL_TEST_KEY) && defined(HAVE_CAL_CI_KEY)
#error "CAL key contradiction, two alternative keys selected at once"
#endif

static const uint8_t LEDGER_SIGNATURE_PUBLIC_KEY[] = {
#if defined(HAVE_CAL_TEST_KEY)
    // test key 2019-01-11 03:07PM (erc20signer)
    0x04, 0x20, 0xda, 0x62, 0x00, 0x3c, 0x0c, 0xe0, 0x97, 0xe3, 0x36, 0x44, 0xa1, 0x0f,
    0xe4, 0xc3, 0x04, 0x54, 0x06, 0x9a, 0x44, 0x54, 0xf0, 0xfa, 0x9d, 0x4e, 0x84, 0xf4,
    0x50, 0x91, 0x42, 0x9b, 0x52, 0x20, 0xaf, 0x9e, 0x35, 0xc0, 0xb2, 0xd9, 0x28, 0x93,
    0x80, 0x13, 0x73, 0x07, 0xde, 0x4d, 0xd1, 0xd4, 0x18, 0x42, 0x8c, 0xf2, 0x1a, 0x93,
    0xb3, 0x35, 0x61, 0xbb, 0x09, 0xd8, 0x8f, 0xe5, 0x79
#elif defined(HAVE_CAL_CI_KEY)
    0x04, 0x4c, 0xca, 0x8f, 0xad, 0x49, 0x6a, 0xa5, 0x04, 0x0a, 0x00, 0xa7, 0xeb, 0x2f,
    0x5c, 0xc3, 0xb8, 0x53, 0x76, 0xd8, 0x8b, 0xa1, 0x47, 0xa7, 0xd7, 0x05, 0x4a, 0x99,
    0xc6, 0x40, 0x56, 0x18, 0x87, 0xfe, 0x17, 0xa0, 0x96, 0xe3, 0x6c, 0x3b, 0x52, 0x3b,
    0x24, 0x4f, 0x3e, 0x2f, 0xf7, 0xf8, 0x40, 0xae, 0x26, 0xc4, 0xe7, 0x7a, 0xd3, 0xbc,
    0x73, 0x9a, 0xf5, 0xde, 0x6f, 0x2d, 0x77, 0xa7, 0xb6
#else
    // production key 2019-01-11 03:07PM (erc20signer)
    0x04, 0x5e, 0x6c, 0x10, 0x20, 0xc1, 0x4d, 0xc4, 0x64, 0x42, 0xfe, 0x89, 0xf9, 0x7c,
    0x0b, 0x68, 0xcd, 0xb1, 0x59, 0x76, 0xdc, 0x24, 0xf2, 0x4c, 0x31, 0x6e, 0x7b, 0x30,
    0xfe, 0x4e, 0x8c, 0xc7, 0x6b, 0x14, 0x89, 0x15, 0x0c, 0x21, 0x51, 0x4e, 0xbf, 0x44,
    0x0f, 0xf5, 0xde, 0xa5, 0x39, 0x3d, 0x83, 0xde, 0x53, 0x58, 0xcd, 0x09, 0x8f, 0xce,
    0x8f, 0xd0, 0xf8, 0x1d, 0xaa, 0x94, 0x97, 0x91, 0x83
#endif
};

#else

// These defines need to follow tokens.c

#define NUM_TOKENS_AKROMA           0
#define NUM_TOKENS_ELLAISM          1
#define NUM_TOKENS_ETHEREUM         2
#define NUM_TOKENS_ETHEREUM_CLASSIC 4
#define NUM_TOKENS_ETHERSOCIAL      0
#define NUM_TOKENS_ETHER1           0
#define NUM_TOKENS_PIRL             0
#define NUM_TOKENS_POA              0
#define NUM_TOKENS_RSK              0
#define NUM_TOKENS_UBIQ             6
#define NUM_TOKENS_EXPANSE          0
#define NUM_TOKENS_WANCHAIN         0
#define NUM_TOKENS_KUSD             0
#define NUM_TOKENS_MUSICOIN         0
#define NUM_TOKENS_CALLISTO         0
#define NUM_TOKENS_ETHERGEM         0
#define NUM_TOKENS_ATHEIOS          0
#define NUM_TOKENS_GOCHAIN          0
#define NUM_TOKENS_MIX              0
#define NUM_TOKENS_REOSC            0
#define NUM_TOKENS_HPB              0
#define NUM_TOKENS_TOMOCHAIN        0
#define NUM_TOKENS_TOBALABA         0
#define NUM_TOKENS_DEXON            0
#define NUM_TOKENS_VOLTA            0
#define NUM_TOKENS_ENERGYWEBCHAIN   0
#define NUM_TOKENS_ARTIS_SIGMA1     0
#define NUM_TOKENS_ARTIS_TAU1       0
#define NUM_TOKENS_WEBCHAIN         0
#define NUM_TOKENS_THUNDERCORE      0
#define NUM_TOKENS_FLARE            0
#define NUM_TOKENS_BSC              0
#define NUM_TOKENS_SONGBIRD         0
#define NUM_TOKENS_MOONRIVER        0
#define NUM_TOKENS_POLYGON          0
#define NUM_TOKENS_SHYFT            0
#define NUM_TOKENS_CONFLUX_ESPACE   0
#define NUM_TOKENS_MOONBEAM         0
#define NUM_TOKENS_BTTC             0
#define NUM_TOKENS_KARDIACHAIN      0
#define NUM_TOKENS_WETHIO           0
#define NUM_TOKENS_OKC              0
#define NUM_TOKENS_CUBE             0
#define NUM_TOKENS_ASTAR            0
#define NUM_TOKENS_SHIDEN           0
#define NUM_TOKENS_XDCNETWORK       0
#define NUM_TOKENS_METER            0
#define NUM_TOKENS_MULTIVAC         0
#define NUM_TOKENS_TECRA            0
#define NUM_TOKENS_APOTHEMNETWORK   0
#define NUM_TOKENS_OASYS            0

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
extern tokenDefinition_t const TOKENS_DEXON[NUM_TOKENS_DEXON];
extern tokenDefinition_t const TOKENS_VOLTA[NUM_TOKENS_VOLTA];
extern tokenDefinition_t const TOKENS_ENERGYWEBCHAIN[NUM_TOKENS_ENERGYWEBCHAIN];
extern tokenDefinition_t const TOKENS_ARTIS_SIGMA1[NUM_TOKENS_ARTIS_SIGMA1];
extern tokenDefinition_t const TOKENS_ARTIS_TAU1[NUM_TOKENS_ARTIS_TAU1];
extern tokenDefinition_t const TOKENS_WEBCHAIN[NUM_TOKENS_WEBCHAIN];
extern tokenDefinition_t const TOKENS_THUNDERCORE[NUM_TOKENS_THUNDERCORE];
extern tokenDefinition_t const TOKENS_FLARE[NUM_TOKENS_FLARE];
extern tokenDefinition_t const TOKENS_BSC[NUM_TOKENS_BSC];
extern tokenDefinition_t const TOKENS_SONGBIRD[NUM_TOKENS_SONGBIRD];
extern tokenDefinition_t const TOKENS_MOONRIVER[NUM_TOKENS_MOONRIVER];
extern tokenDefinition_t const TOKENS_POLYGON[NUM_TOKENS_POLYGON];
extern tokenDefinition_t const TOKENS_SHYFT[NUM_TOKENS_SHYFT];
extern tokenDefinition_t const TOKENS_CONFLUX_ESPACE[NUM_TOKENS_CONFLUX_ESPACE];
extern tokenDefinition_t const TOKENS_MOONBEAM[NUM_TOKENS_MOONBEAM];
extern tokenDefinition_t const TOKENS_BTTC[NUM_TOKENS_BTTC];
extern tokenDefinition_t const TOKENS_KARDIACHAIN[NUM_TOKENS_KARDIACHAIN];
extern tokenDefinition_t const TOKENS_WETHIO[NUM_TOKENS_WETHIO];
extern tokenDefinition_t const TOKENS_CUBE[NUM_TOKENS_CUBE];
extern tokenDefinition_t const TOKENS_ASTAR[NUM_TOKENS_ASTAR];
extern tokenDefinition_t const TOKENS_SHIDEN[NUM_TOKENS_SHIDEN];
extern tokenDefinition_t const TOKENS_XDCNETWORK[NUM_TOKENS_XDCNETWORK];
extern tokenDefinition_t const TOKENS_METER[NUM_TOKENS_METER];
extern tokenDefinition_t const TOKENS_MULTIVAC[NUM_TOKENS_MULTIVAC];
extern tokenDefinition_t const TOKENS_TECRA[NUM_TOKENS_TECRA];
extern tokenDefinition_t const TOKENS_APOTHEMNETWORK[NUM_TOKENS_APOTHEMNETWORK];
extern tokenDefinition_t const TOKENS_OASYS[NUM_TOKENS_OASYS];

#endif /* HAVE_TOKENS_LIST */

#endif  // _TOKENS_H_
