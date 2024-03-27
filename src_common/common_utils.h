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

#include <stddef.h>
#include <stdint.h>

#include "os.h"
#include "cx.h"

#define WEI_TO_ETHER 18

#define ADDRESS_LENGTH 20
#define INT128_LENGTH  16
#define INT256_LENGTH  32

#define KECCAK256_HASH_BYTESIZE 32

static const char HEXDIGITS[] = "0123456789abcdef";

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void array_hexstr(char *strbuf, const void *bin, unsigned int len);

uint64_t u64_from_BE(const uint8_t *in, uint8_t size);

bool u64_to_string(uint64_t src, char *dst, uint8_t dst_size);

bool uint256_to_decimal(const uint8_t *value, size_t value_len, char *out, size_t out_len);

bool amountToString(const uint8_t *amount,
                    uint8_t amount_len,
                    uint8_t decimals,
                    const char *ticker,
                    char *out_buffer,
                    size_t out_buffer_size);

bool adjustDecimals(const char *src,
                    size_t srcLength,
                    char *target,
                    size_t targetLength,
                    uint8_t decimals);

void getEthAddressFromRawKey(const uint8_t raw_pubkey[static 65],
                             uint8_t out[static ADDRESS_LENGTH]);

void getEthAddressStringFromRawKey(const uint8_t raw_pubkey[static 65],
                                   char out[static ADDRESS_LENGTH * 2],
                                   uint64_t chainId);

bool getEthAddressStringFromBinary(uint8_t *address,
                                   char out[static ADDRESS_LENGTH * 2],
                                   uint64_t chainId);

bool getEthDisplayableAddress(uint8_t *in, char *out, size_t out_len, uint64_t chainId);

static __attribute__((no_instrument_function)) inline int allzeroes(const void *buf, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    for (size_t i = 0; i < n; ++i) {
        if (p[i]) {
            return 0;
        }
    }
    return 1;
}
static __attribute__((no_instrument_function)) inline int ismaxint(uint8_t *buf, int n) {
    for (int i = 0; i < n; ++i) {
        if (buf[i] != 0xff) {
            return 0;
        }
    }
    return 1;
}
