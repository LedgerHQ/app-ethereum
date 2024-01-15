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

#include <stdint.h>
#include <string.h>

#include "ethUtils.h"
#include "uint128.h"
#include "uint256.h"
#include "tokens.h"
#include "utils.h"

void array_hexstr(char *strbuf, const void *bin, unsigned int len) {
    while (len--) {
        *strbuf++ = HEXDIGITS[((*((char *) bin)) >> 4) & 0xF];
        *strbuf++ = HEXDIGITS[(*((char *) bin)) & 0xF];
        bin = (const void *) ((unsigned int) bin + 1);
    }
    *strbuf = 0;  // EOS
}

void convertUint64BEto128(const uint8_t *const data, uint32_t length, uint128_t *const target) {
    uint8_t tmp[INT128_LENGTH];
    int64_t value;

    value = u64_from_BE(data, length);
    memset(tmp, ((value < 0) ? 0xff : 0), sizeof(tmp) - length);
    memmove(tmp + sizeof(tmp) - length, data, length);
    readu128BE(tmp, target);
}

void convertUint128BE(const uint8_t *const data, uint32_t length, uint128_t *const target) {
    uint8_t tmp[INT128_LENGTH];

    memset(tmp, 0, sizeof(tmp) - length);
    memmove(tmp + sizeof(tmp) - length, data, length);
    readu128BE(tmp, target);
}

void convertUint256BE(const uint8_t *const data, uint32_t length, uint256_t *const target) {
    uint8_t tmp[INT256_LENGTH];

    memset(tmp, 0, sizeof(tmp) - length);
    memmove(tmp + sizeof(tmp) - length, data, length);
    readu256BE(tmp, target);
}

uint64_t u64_from_BE(const uint8_t *in, uint8_t size) {
    uint8_t i = 0;
    uint64_t res = 0;

    while (i < size && i < sizeof(res)) {
        res <<= 8;
        res |= in[i];
        i++;
    }

    return res;
}

bool u64_to_string(uint64_t src, char *dst, uint8_t dst_size) {
    // Copy the numbers in ASCII format.
    uint8_t i = 0;
    do {
        // Checking `i + 1` to make sure we have enough space for '\0'.
        if (i + 1 >= dst_size) {
            return false;
        }
        dst[i] = src % 10 + '0';
        src /= 10;
        i++;
    } while (src);

    // Null terminate string
    dst[i] = '\0';

    // Revert the string
    i--;
    uint8_t j = 0;
    while (j < i) {
        char tmp = dst[i];
        dst[i] = dst[j];
        dst[j] = tmp;
        i--;
        j++;
    }
    return true;
}

bool uint256_to_decimal(const uint8_t *value, size_t value_len, char *out, size_t out_len) {
    if (value_len > INT256_LENGTH) {
        // value len is bigger than INT256_LENGTH ?!
        return false;
    }

    uint16_t n[16] = {0};
    // Copy and right-align the number
    memcpy((uint8_t *) n + INT256_LENGTH - value_len, value, value_len);

    // Special case when value is 0
    if (allzeroes(n, INT256_LENGTH)) {
        if (out_len < 2) {
            // Not enough space to hold "0" and \0.
            return false;
        }
        strlcpy(out, "0", out_len);
        return true;
    }

    uint16_t *p = n;
    for (int i = 0; i < 16; i++) {
        n[i] = __builtin_bswap16(*p++);
    }
    int pos = out_len;
    while (!allzeroes(n, sizeof(n))) {
        if (pos == 0) {
            return false;
        }
        pos -= 1;
        unsigned int carry = 0;
        for (int i = 0; i < 16; i++) {
            int rem = ((carry << 16) | n[i]) % 10;
            n[i] = ((carry << 16) | n[i]) / 10;
            carry = rem;
        }
        out[pos] = '0' + carry;
    }
    memmove(out, out + pos, out_len - pos);
    out[out_len - pos] = 0;
    return true;
}

bool adjustDecimals(const char *src,
                    size_t srcLength,
                    char *target,
                    size_t targetLength,
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

bool amountToString(const uint8_t *amount,
                    uint8_t amount_size,
                    uint8_t decimals,
                    const char *ticker,
                    char *out_buffer,
                    size_t out_buffer_size) {
    char tmp_buffer[100] = {0};

    if (uint256_to_decimal(amount, amount_size, tmp_buffer, sizeof(tmp_buffer)) == false) {
        return false;
    }

    uint8_t amount_len = strnlen(tmp_buffer, sizeof(tmp_buffer));
    uint8_t ticker_len = strnlen(ticker, MAX_TICKER_LEN);

    memcpy(out_buffer, ticker, MIN(out_buffer_size, ticker_len));
    if (ticker_len > 0) {
        out_buffer[ticker_len++] = ' ';
    }

    if (adjustDecimals(tmp_buffer,
                       amount_len,
                       out_buffer + ticker_len,
                       out_buffer_size - ticker_len - 1,
                       decimals) == false) {
        return false;
    }

    out_buffer[out_buffer_size - 1] = '\0';
    return true;
}


bool getEthAddressFromKey(cx_ecfp_public_key_t *publicKey, uint8_t *out, cx_sha3_t *sha3Context) {
    uint8_t hashAddress[INT256_LENGTH];

    if (cx_keccak_init_no_throw(sha3Context, 256) != CX_OK) {
        return false;
    }

    if (cx_hash_no_throw((cx_hash_t *) sha3Context,
                         CX_LAST,
                         publicKey->W + 1,
                         64,
                         hashAddress,
                         32) != CX_OK) {
        return false;
    }

    memmove(out, hashAddress + 12, 20);
    return true;
}

bool getEthAddressStringFromKey(cx_ecfp_public_key_t *publicKey,
                                char *out,
                                cx_sha3_t *sha3Context,
                                uint64_t chainId) {
    uint8_t hashAddress[INT256_LENGTH];

    if (cx_keccak_init_no_throw(sha3Context, 256) != CX_OK) {
        return false;
    }

    if (cx_hash_no_throw((cx_hash_t *) sha3Context,
                         CX_LAST,
                         publicKey->W + 1,
                         64,
                         hashAddress,
                         32) != CX_OK) {
        return false;
    }

    if (!getEthAddressStringFromBinary(hashAddress + 12, out, sha3Context, chainId)) {
        return false;
    }

    return true;
}

bool getEthAddressStringFromBinary(uint8_t *address,
                                   char *out,
                                   cx_sha3_t *sha3Context,
                                   uint64_t chainId) {
    // save some precious stack space
    union locals_union {
        uint8_t hashChecksum[INT256_LENGTH];
        uint8_t tmp[51];
    } locals_union;

    uint8_t i;
    bool eip1191 = false;
    uint32_t offset = 0;
    switch (chainId) {
        case 30:
        case 31:
            eip1191 = true;
            break;
    }
    if (eip1191) {
        if (!u64_to_string(chainId, (char *) locals_union.tmp, sizeof(locals_union.tmp))) {
            return false;
        }
        offset = strnlen((char *) locals_union.tmp, sizeof(locals_union.tmp));
        strlcat((char *) locals_union.tmp + offset, "0x", sizeof(locals_union.tmp) - offset);
        offset = strnlen((char *) locals_union.tmp, sizeof(locals_union.tmp));
    }
    for (i = 0; i < 20; i++) {
        uint8_t digit = address[i];
        locals_union.tmp[offset + 2 * i] = HEXDIGITS[(digit >> 4) & 0x0f];
        locals_union.tmp[offset + 2 * i + 1] = HEXDIGITS[digit & 0x0f];
    }
    if (cx_keccak_init_no_throw(sha3Context, 256) != CX_OK) {
        return false;
    }

    if (cx_hash_no_throw((cx_hash_t *) sha3Context,
                         CX_LAST,
                         locals_union.tmp,
                         offset + 40,
                         locals_union.hashChecksum,
                         32) != CX_OK) {
        return false;
    }
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

    return true;
}

/* Fills the `out` buffer with the lowercase string representation of the pubkey passed in as binary
format by `in`. (eg: uint8_t*:0xb47e3cd837dDF8e4c57F05d70Ab865de6e193BBB ->
char*:"0xb47e3cd837dDF8e4c57F05d70Ab865de6e193BBB\0" )
`sha3` context doesn't have have to be initialized prior to call.*/
bool getEthDisplayableAddress(uint8_t *in,
                              char *out,
                              size_t out_len,
                              cx_sha3_t *sha3,
                              uint64_t chainId) {
    if (out_len < 43) {
        strlcpy(out, "ERROR", out_len);
        return false;
    }
    out[0] = '0';
    out[1] = 'x';
    if (!getEthAddressStringFromBinary(in, out + 2, sha3, chainId)) {
        strlcpy(out, "ERROR", out_len);
        return false;
    }

    return true;
}
