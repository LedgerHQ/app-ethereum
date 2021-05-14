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

/**
 * @brief Utilities for an Ethereum Hardware Wallet logic
 * @file ethUtils.h
 * @author Ledger Firmware Team <hello@ledger.fr>
 * @version 1.0
 * @date 8th of March 2016
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "os.h"
#include "cx.h"
#include "ethUtils.h"
#include "chainConfig.h"

bool rlpCanDecode(uint8_t *buffer, uint32_t bufferLength, bool *valid) {
    if (*buffer <= 0x7f) {
    } else if (*buffer <= 0xb7) {
    } else if (*buffer <= 0xbf) {
        if (bufferLength < (1 + (*buffer - 0xb7))) {
            return false;
        }
        if (*buffer > 0xbb) {
            *valid = false;  // arbitrary 32 bits length limitation
            return true;
        }
    } else if (*buffer <= 0xf7) {
    } else {
        if (bufferLength < (1 + (*buffer - 0xf7))) {
            return false;
        }
        if (*buffer > 0xfb) {
            *valid = false;  // arbitrary 32 bits length limitation
            return true;
        }
    }
    *valid = true;
    return true;
}

bool rlpDecodeLength(uint8_t *buffer,
                     uint32_t bufferLength,
                     uint32_t *fieldLength,
                     uint32_t *offset,
                     bool *list) {
    if (*buffer <= 0x7f) {
        *offset = 0;
        *fieldLength = 1;
        *list = false;
    } else if (*buffer <= 0xb7) {
        *offset = 1;
        *fieldLength = *buffer - 0x80;
        *list = false;
    } else if (*buffer <= 0xbf) {
        *offset = 1 + (*buffer - 0xb7);
        *list = false;
        switch (*buffer) {
            case 0xb8:
                *fieldLength = *(buffer + 1);
                break;
            case 0xb9:
                *fieldLength = (*(buffer + 1) << 8) + *(buffer + 2);
                break;
            case 0xba:
                *fieldLength = (*(buffer + 1) << 16) + (*(buffer + 2) << 8) + *(buffer + 3);
                break;
            case 0xbb:
                *fieldLength = (*(buffer + 1) << 24) + (*(buffer + 2) << 16) +
                               (*(buffer + 3) << 8) + *(buffer + 4);
                break;
            default:
                return false;  // arbitrary 32 bits length limitation
        }
    } else if (*buffer <= 0xf7) {
        *offset = 1;
        *fieldLength = *buffer - 0xc0;
        *list = true;
    } else {
        *offset = 1 + (*buffer - 0xf7);
        *list = true;
        switch (*buffer) {
            case 0xf8:
                *fieldLength = *(buffer + 1);
                break;
            case 0xf9:
                *fieldLength = (*(buffer + 1) << 8) + *(buffer + 2);
                break;
            case 0xfa:
                *fieldLength = (*(buffer + 1) << 16) + (*(buffer + 2) << 8) + *(buffer + 3);
                break;
            case 0xfb:
                *fieldLength = (*(buffer + 1) << 24) + (*(buffer + 2) << 16) +
                               (*(buffer + 3) << 8) + *(buffer + 4);
                break;
            default:
                return false;  // arbitrary 32 bits length limitation
        }
    }

    return true;
}

void getEthAddressFromKey(cx_ecfp_public_key_t *publicKey, uint8_t *out, cx_sha3_t *sha3Context) {
    uint8_t hashAddress[32];
    cx_keccak_init(sha3Context, 256);
    cx_hash((cx_hash_t *) sha3Context, CX_LAST, publicKey->W + 1, 64, hashAddress, 32);
    memmove(out, hashAddress + 12, 20);
}

#ifdef CHECKSUM_1

static const uint8_t const HEXDIGITS[] = "0123456789ABCDEF";

static const uint8_t const MASK[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

char convertDigit(uint8_t *address, uint8_t index, uint8_t *hash) {
    unsigned char digit = address[index / 2];
    if ((index % 2) == 0) {
        digit = (digit >> 4) & 0x0f;
    } else {
        digit = digit & 0x0f;
    }
    if (digit < 10) {
        return HEXDIGITS[digit];
    } else {
        unsigned char data = hash[index / 8];
        if (((data & MASK[index % 8]) != 0) && (digit > 9)) {
            return HEXDIGITS[digit] /*- 'a' + 'A'*/;
        } else {
            return HEXDIGITS[digit];
        }
    }
}

void getEthAddressStringFromKey(cx_ecfp_public_key_t *publicKey,
                                uint8_t *out,
                                cx_sha3_t *sha3Context,
                                chain_config_t *chain_config) {
    uint8_t hashAddress[32];
    cx_keccak_init(sha3Context, 256);
    cx_hash((cx_hash_t *) sha3Context, CX_LAST, publicKey->W + 1, 64, hashAddress, 32);
    getEthAddressStringFromBinary(hashAddress + 12, out, sha3Context, chain_config);
}

void getEthAddressStringFromBinary(uint8_t *address,
                                   uint8_t *out,
                                   cx_sha3_t *sha3Context,
                                   chain_config_t *chain_config) {
    UNUSED(chain_config);
    uint8_t hashChecksum[32];
    uint8_t i;
    cx_keccak_init(sha3Context, 256);
    cx_hash((cx_hash_t *) sha3Context, CX_LAST, address, 20, hashChecksum, 32);
    for (i = 0; i < 40; i++) {
        out[i] = convertDigit(address, i, hashChecksum);
    }
    out[40] = '\0';
}

#else

static const uint8_t const HEXDIGITS[] = "0123456789abcdef";

void getEthAddressStringFromKey(cx_ecfp_public_key_t *publicKey,
                                uint8_t *out,
                                cx_sha3_t *sha3Context,
                                chain_config_t *chain_config) {
    uint8_t hashAddress[32];
    cx_keccak_init(sha3Context, 256);
    cx_hash((cx_hash_t *) sha3Context, CX_LAST, publicKey->W + 1, 64, hashAddress, 32);
    getEthAddressStringFromBinary(hashAddress + 12, out, sha3Context, chain_config);
}

void getThetaTxFromBinary(txContext_t *context, cx_sha3_t *sha3Context, chain_config_t *chain_config) {
    char out[300];
    uint32_t offset;
    uint8_t i, thetaOffset = 12, pos = context->content->thetaTXLength - thetaOffset;
    bool valid, canDecode, decoded;
    uint8_t *rlpP;
    uint32_t tmpCurrentFieldLength, tmpCurrentFieldPos;
    bool tmpCurrentFieldIsList;
    BEGIN_TRY {
        TRY {
            rlpP = context->content->thetaTX + thetaOffset;
            for (i = 0; i < (context->content->thetaTXLength - thetaOffset) * 2; i++) {
                uint8_t digit = rlpP[i / 2];
                if ((i % 2) == 0) {
                    digit = (digit >> 4) & 0x0f;
                } else {
                    digit = digit & 0x0f;
                }
                out[i] = HEXDIGITS[digit];
            }
            out[i] = '\0';
            if (context->content->thetaTXLength >= thetaOffset) {
                canDecode = rlpCanDecode(rlpP, pos, &valid);
                if (canDecode) {
                    decoded = rlpDecodeLength(rlpP, pos, &context->content->thetaCurrentFieldLength, &offset, &context->content->thetaCurrentFieldIsList);
                    if (decoded && context->content->thetaCurrentFieldIsList) {
                        rlpP += offset;
                        decoded = rlpDecodeLength(rlpP, pos, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList);
                        if (decoded && tmpCurrentFieldIsList) {
                            //TODO : get gas fee
                            rlpP += offset + tmpCurrentFieldLength;
                        } else {
                            return;
                        }
                        decoded = rlpDecodeLength(rlpP, pos, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList);
                        if (decoded && tmpCurrentFieldIsList) {
                            rlpP += offset + tmpCurrentFieldLength;
                        } else {
                            return;
                        }
                        decoded = rlpDecodeLength(rlpP, pos, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList);
                        if (decoded && tmpCurrentFieldIsList) {
                            //copy address
                            if (*(rlpP + 2) == 148) {
                                rlpP += 3;
                                memmove(context->content->destination, rlpP, 20);
                            }
                            //copy value
                            rlpP += 21;
                            if (*rlpP != 128 && rlpDecodeLength(rlpP, pos, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList)) {//Theta is not 0
                                rlpP++;                          
                                memmove(context->content->value.value, rlpP, tmpCurrentFieldLength);
                                context->content->value.length = tmpCurrentFieldLength;
                            } else if (rlpDecodeLength(++rlpP, pos, &tmpCurrentFieldLength, &offset, &tmpCurrentFieldIsList)) { // send Tfuel
                                context->content->thetaTXtoken = 1;
                                memmove(context->content->value.value, rlpP + 1, tmpCurrentFieldLength);
                                context->content->value.length = tmpCurrentFieldLength;
                            }
                        }
                    }
                }
            }
        }
        CATCH_OTHER(e) {
            PRINTF("Get ThetaTx failed \n");
        }
        FINALLY {
        }
    }
    END_TRY;
};

void getEthAddressStringFromBinary(uint8_t *address,
                                   uint8_t *out,
                                   cx_sha3_t *sha3Context,
                                   chain_config_t *chain_config) {
    // save some precious stack space
    union locals_union {
        uint8_t hashChecksum[32];
        uint8_t tmp[51];
    } locals_union;

    uint8_t i;
    bool eip1191 = false;
    uint32_t offset = 0;
    switch (chain_config->chainId) {
        case 30:
        case 31:
            eip1191 = true;
            break;
    }
    if (eip1191) {
        snprintf((char *) locals_union.tmp,
                 sizeof(locals_union.tmp),
                 "%d0x",
                 chain_config->chainId);
        offset = strlen((char *) locals_union.tmp);
    }
    for (i = 0; i < 20; i++) {
        uint8_t digit = address[i];
        locals_union.tmp[offset + 2 * i] = HEXDIGITS[(digit >> 4) & 0x0f];
        locals_union.tmp[offset + 2 * i + 1] = HEXDIGITS[digit & 0x0f];
    }
    cx_keccak_init(sha3Context, 256);
    cx_hash((cx_hash_t *) sha3Context,
            CX_LAST,
            locals_union.tmp,
            offset + 40,
            locals_union.hashChecksum,
            32);
    for (i = 0; i < 40; i++) {
        uint8_t digit = address[i / 2];
        if ((i % 2) == 0) {
            digit = (digit >> 4) & 0x0f;
        } else {
            digit = digit & 0x0f;
        }
        if (digit < 10) {
            out[i] = HEXDIGITS[digit];
        } else {
            int v = (locals_union.hashChecksum[i / 2] >> (4 * (1 - i % 2))) & 0x0f;
            if (v >= 8) {
                out[i] = HEXDIGITS[digit] - 'a' + 'A';
            } else {
                out[i] = HEXDIGITS[digit];
            }
        }
    }
    out[40] = '\0';
}

#endif

bool adjustDecimals(char *src,
                    uint32_t srcLength,
                    char *target,
                    uint32_t targetLength,
                    uint8_t decimals) {
    uint32_t startOffset;
    uint32_t lastZeroOffset = 0;
    uint32_t offset = 0;
    if ((srcLength == 1) && (*src == '0')) {
        if (targetLength < 2) {
            return false;
        }
        target[0] = '0';
        target[1] = '\0';
        return true;
    }
    if (srcLength <= decimals) {
        uint32_t delta = decimals - srcLength;
        if (targetLength < srcLength + 1 + 2 + delta) {
            return false;
        }
        target[offset++] = '0';
        target[offset++] = '.';
        for (uint32_t i = 0; i < delta; i++) {
            target[offset++] = '0';
        }
        startOffset = offset;
        for (uint32_t i = 0; i < srcLength; i++) {
            target[offset++] = src[i];
        }
        target[offset] = '\0';
    } else {
        uint32_t sourceOffset = 0;
        uint32_t delta = srcLength - decimals;
        if (targetLength < srcLength + 1 + 1) {
            return false;
        }
        while (offset < delta) {
            target[offset++] = src[sourceOffset++];
        }
        if (decimals != 0) {
            target[offset++] = '.';
        }
        startOffset = offset;
        while (sourceOffset < srcLength) {
            target[offset++] = src[sourceOffset++];
        }
        target[offset] = '\0';
    }
    for (uint32_t i = startOffset; i < offset; i++) {
        if (target[i] == '0') {
            if (lastZeroOffset == 0) {
                lastZeroOffset = i;
            }
        } else {
            lastZeroOffset = 0;
        }
    }
    if (lastZeroOffset != 0) {
        target[lastZeroOffset] = '\0';
        if (target[lastZeroOffset - 1] == '.') {
            target[lastZeroOffset - 1] = '\0';
        }
    }
    return true;
}
