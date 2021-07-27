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

#ifndef _CHAIN_CONFIG_H_
#define _CHAIN_CONFIG_H_

#include <stdint.h>

#include "os.h"

typedef enum chain_kind_e {
    CHAIN_KIND_ETHEREUM,
    CHAIN_KIND_ETHEREUM_CLASSIC,
    CHAIN_KIND_EXPANSE,
    CHAIN_KIND_POA,
    CHAIN_KIND_RSK,
    CHAIN_KIND_UBIQ,
    CHAIN_KIND_WANCHAIN,
    CHAIN_KIND_KUSD,
    CHAIN_KIND_PIRL,
    CHAIN_KIND_AKROMA,
    CHAIN_KIND_MUSICOIN,
    CHAIN_KIND_CALLISTO,
    CHAIN_KIND_ETHERSOCIAL,
    CHAIN_KIND_ELLAISM,
    CHAIN_KIND_ETHER1,
    CHAIN_KIND_ETHERGEM,
    CHAIN_KIND_ATHEIOS,
    CHAIN_KIND_GOCHAIN,
    CHAIN_KIND_MIX,
    CHAIN_KIND_REOSC,
    CHAIN_KIND_HPB,
    CHAIN_KIND_TOMOCHAIN,
    CHAIN_KIND_TOBALABA,
    CHAIN_KIND_DEXON,
    CHAIN_KIND_VOLTA,
    CHAIN_KIND_EWC,
    CHAIN_KIND_ARTIS_SIGMA1,
    CHAIN_KIND_ARTIS_TAU1,
    CHAIN_KIND_WEBCHAIN,
    CHAIN_KIND_THUNDERCORE,
    CHAIN_KIND_FLARE,
    CHAIN_KIND_THETA,
    CHAIN_KIND_BSC
} chain_kind_t;

typedef struct chain_config_s {
    char coinName[10];  // ticker
    uint32_t chainId;
    chain_kind_t kind;
} chain_config_t;

#define ETHEREUM_MAINNET_CHAINID 1

#endif /* _CHAIN_CONFIG_H_ */
