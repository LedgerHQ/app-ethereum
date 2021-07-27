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

#ifndef _ETHUTILS_H_
#define _ETHUTILS_H_

#include <stdint.h>

#include "cx.h"
#include "chainConfig.h"
/**
 * @brief Decode an RLP encoded field - see
 * https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] buffer buffer containing the RLP encoded field to decode
 * @param [out] fieldLength length of the RLP encoded field
 * @param [out] offset offset to the beginning of the RLP encoded field from the
 * buffer
 * @param [out] list true if the field encodes a list, false if it encodes a
 * string
 * @return true if the RLP header is consistent
 */
bool rlpDecodeLength(uint8_t *buffer, uint32_t *fieldLength, uint32_t *offset, bool *list);

bool rlpCanDecode(uint8_t *buffer, uint32_t bufferLength, bool *valid);

void getEthAddressFromKey(cx_ecfp_public_key_t *publicKey, uint8_t *out, cx_sha3_t *sha3Context);

void getEthAddressStringFromKey(cx_ecfp_public_key_t *publicKey,
                                char *out,
                                cx_sha3_t *sha3Context,
                                chain_config_t *chain_config);

void getEthAddressStringFromBinary(uint8_t *address,
                                   char *out,
                                   cx_sha3_t *sha3Context,
                                   chain_config_t *chain_config);

bool adjustDecimals(char *src,
                    uint32_t srcLength,
                    char *target,
                    uint32_t targetLength,
                    uint8_t decimals);

__attribute__((no_instrument_function)) inline int allzeroes(void *buf, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    for (size_t i = 0; i < n; ++i) {
        if (p[i]) {
            return 0;
        }
    }
    return 1;
}
__attribute__((no_instrument_function)) inline int ismaxint(uint8_t *buf, int n) {
    for (int i = 0; i < n; ++i) {
        if (buf[i] != 0xff) {
            return 0;
        }
    }
    return 1;
}

static const char HEXDIGITS[] = "0123456789abcdef";

#endif /* _ETHUTILS_H_ */
