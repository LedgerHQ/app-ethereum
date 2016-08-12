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

/**
 * @brief Decode an RLP encoded field - see
 * https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] buffer buffer containing the RLP encoded field to decode
 * @param [in] bufferLength size of the buffer
 * @param [out] fieldLength length of the RLP encoded field
 * @param [out] offset offset to the beginning of the RLP encoded field from the
 * buffer
 * @param [out] list true if the field encodes a list, false if it encodes a
 * string
 * @return true if the RLP header is consistent
 */
bool rlpDecodeLength(uint8_t *buffer, uint32_t bufferLength,
                     uint32_t *fieldLength, uint32_t *offset, bool *list);

bool rlpCanDecode(uint8_t *buffer, uint32_t bufferLength, bool *valid);

void getEthAddressFromKey(cx_ecfp_public_key_t *publicKey, uint8_t *out,
                          cx_sha3_t *sha3Context);

void getEthAddressStringFromKey(cx_ecfp_public_key_t *publicKey, uint8_t *out,
                                cx_sha3_t *sha3Context);

void getEthAddressStringFromBinary(uint8_t *address, uint8_t *out,
                                   cx_sha3_t *sha3Context);

bool adjustDecimals(char *src, uint32_t srcLength, char *target,
                    uint32_t targetLength, uint8_t decimals);
