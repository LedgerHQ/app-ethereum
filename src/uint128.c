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

// Adapted from https://github.com/calccrypto/uint256_t

#include <stdio.h>
#include <string.h>
#include "read.h"
#include "uint128.h"
#include "uint_common.h"
#include "common_utils.h"  // HEXDIGITS

void readu128BE(const uint8_t *const buffer, uint128_t *const target) {
    UPPER_P(target) = read_u64_be(buffer, 0);
    LOWER_P(target) = read_u64_be(buffer + 8, 0);
}

bool zero128(const uint128_t *const number) {
    return ((LOWER_P(number) == 0) && (UPPER_P(number) == 0));
}

void copy128(uint128_t *const target, const uint128_t *const number) {
    UPPER_P(target) = UPPER_P(number);
    LOWER_P(target) = LOWER_P(number);
}

void clear128(uint128_t *const target) {
    UPPER_P(target) = 0;
    LOWER_P(target) = 0;
}

void shiftl128(const uint128_t *const number, uint32_t value, uint128_t *const target) {
    if (value >= 128) {
        clear128(target);
    } else if (value == 64) {
        UPPER_P(target) = LOWER_P(number);
        LOWER_P(target) = 0;
    } else if (value == 0) {
        copy128(target, number);
    } else if (value < 64) {
        UPPER_P(target) = (UPPER_P(number) << value) + (LOWER_P(number) >> (64 - value));
        LOWER_P(target) = (LOWER_P(number) << value);
    } else {
        UPPER_P(target) = LOWER_P(number) << (value - 64);
        LOWER_P(target) = 0;
    }
}

void shiftr128(const uint128_t *const number, uint32_t value, uint128_t *const target) {
    if (value >= 128) {
        clear128(target);
    } else if (value == 64) {
        UPPER_P(target) = 0;
        LOWER_P(target) = UPPER_P(number);
    } else if (value == 0) {
        copy128(target, number);
    } else if (value < 64) {
        uint128_t result;
        UPPER(result) = UPPER_P(number) >> value;
        LOWER(result) = (UPPER_P(number) << (64 - value)) + (LOWER_P(number) >> value);
        copy128(target, &result);
    } else {
        LOWER_P(target) = UPPER_P(number) >> (value - 64);
        UPPER_P(target) = 0;
    }
}

uint32_t bits128(const uint128_t *const number) {
    uint32_t result = 0;
    if (UPPER_P(number)) {
        result = 64;
        uint64_t up = UPPER_P(number);
        while (up) {
            up >>= 1;
            result++;
        }
    } else {
        uint64_t low = LOWER_P(number);
        while (low) {
            low >>= 1;
            result++;
        }
    }
    return result;
}

bool equal128(const uint128_t *const number1, const uint128_t *const number2) {
    return (UPPER_P(number1) == UPPER_P(number2)) && (LOWER_P(number1) == LOWER_P(number2));
}

bool gt128(const uint128_t *const number1, const uint128_t *const number2) {
    if (UPPER_P(number1) == UPPER_P(number2)) {
        return (LOWER_P(number1) > LOWER_P(number2));
    }
    return (UPPER_P(number1) > UPPER_P(number2));
}

bool gte128(const uint128_t *const number1, const uint128_t *const number2) {
    return gt128(number1, number2) || equal128(number1, number2);
}

void add128(const uint128_t *const number1,
            const uint128_t *const number2,
            uint128_t *const target) {
    UPPER_P(target) = UPPER_P(number1) + UPPER_P(number2) +
                      ((LOWER_P(number1) + LOWER_P(number2)) < LOWER_P(number1));
    LOWER_P(target) = LOWER_P(number1) + LOWER_P(number2);
}

void sub128(const uint128_t *const number1,
            const uint128_t *const number2,
            uint128_t *const target) {
    UPPER_P(target) = UPPER_P(number1) - UPPER_P(number2) -
                      ((LOWER_P(number1) - LOWER_P(number2)) > LOWER_P(number1));
    LOWER_P(target) = LOWER_P(number1) - LOWER_P(number2);
}

void or128(const uint128_t *const number1,
           const uint128_t *const number2,
           uint128_t *const target) {
    UPPER_P(target) = UPPER_P(number1) | UPPER_P(number2);
    LOWER_P(target) = LOWER_P(number1) | LOWER_P(number2);
}

void mul128(const uint128_t *const number1,
            const uint128_t *const number2,
            uint128_t *const target) {
    uint64_t top[4] = {UPPER_P(number1) >> 32,
                       UPPER_P(number1) & 0xffffffff,
                       LOWER_P(number1) >> 32,
                       LOWER_P(number1) & 0xffffffff};
    uint64_t bottom[4] = {UPPER_P(number2) >> 32,
                          UPPER_P(number2) & 0xffffffff,
                          LOWER_P(number2) >> 32,
                          LOWER_P(number2) & 0xffffffff};
    uint64_t products[4][4];
    uint128_t tmp, tmp2;

    for (int y = 3; y > -1; y--) {
        for (int x = 3; x > -1; x--) {
            products[3 - x][y] = top[x] * bottom[y];
        }
    }

    uint64_t fourth32 = products[0][3] & 0xffffffff;
    uint64_t third32 = (products[0][2] & 0xffffffff) + (products[0][3] >> 32);
    uint64_t second32 = (products[0][1] & 0xffffffff) + (products[0][2] >> 32);
    uint64_t first32 = (products[0][0] & 0xffffffff) + (products[0][1] >> 32);

    third32 += products[1][3] & 0xffffffff;
    second32 += (products[1][2] & 0xffffffff) + (products[1][3] >> 32);
    first32 += (products[1][1] & 0xffffffff) + (products[1][2] >> 32);

    second32 += products[2][3] & 0xffffffff;
    first32 += (products[2][2] & 0xffffffff) + (products[2][3] >> 32);

    first32 += products[3][3] & 0xffffffff;

    UPPER(tmp) = first32 << 32;
    LOWER(tmp) = 0;
    UPPER(tmp2) = third32 >> 32;
    LOWER(tmp2) = third32 << 32;
    add128(&tmp, &tmp2, target);
    UPPER(tmp) = second32;
    LOWER(tmp) = 0;
    add128(&tmp, target, &tmp2);
    UPPER(tmp) = 0;
    LOWER(tmp) = fourth32;
    add128(&tmp, &tmp2, target);
}

void divmod128(const uint128_t *const l,
               const uint128_t *const r,
               uint128_t *const retDiv,
               uint128_t *const retMod) {
    uint128_t copyd, adder, resDiv, resMod;
    uint128_t one;
    UPPER(one) = 0;
    LOWER(one) = 1;
    uint32_t diffBits = bits128(l) - bits128(r);
    clear128(&resDiv);
    copy128(&resMod, l);
    if (gt128(r, l)) {
        copy128(retMod, l);
        clear128(retDiv);
    } else {
        shiftl128(r, diffBits, &copyd);
        shiftl128(&one, diffBits, &adder);
        if (gt128(&copyd, &resMod)) {
            shiftr128(&copyd, 1, &copyd);
            shiftr128(&adder, 1, &adder);
        }
        while (gte128(&resMod, r)) {
            if (gte128(&resMod, &copyd)) {
                sub128(&resMod, &copyd, &resMod);
                or128(&resDiv, &adder, &resDiv);
            }
            shiftr128(&copyd, 1, &copyd);
            shiftr128(&adder, 1, &adder);
        }
        copy128(retDiv, &resDiv);
        copy128(retMod, &resMod);
    }
}

bool tostring128(const uint128_t *const number,
                 uint32_t baseParam,
                 char *const out,
                 uint32_t outLength) {
    uint128_t rDiv;
    uint128_t rMod;
    uint128_t base;
    copy128(&rDiv, number);
    clear128(&rMod);
    clear128(&base);
    LOWER(base) = baseParam;
    uint32_t offset = 0;
    if ((baseParam < 2) || (baseParam > 16)) {
        return false;
    }
    do {
        if (offset > (outLength - 1)) {
            return false;
        }
        divmod128(&rDiv, &base, &rDiv, &rMod);
        out[offset++] = HEXDIGITS[(uint8_t) LOWER(rMod)];
    } while (!zero128(&rDiv));

    if (offset > (outLength - 1)) {
        return false;
    }

    out[offset] = '\0';
    reverseString(out, offset);
    return true;
}

/**
 * Format a uint128_t into a string as a signed integer
 *
 * @param[in] number the number to format
 * @param[in] base the radix used in formatting
 * @param[out] out the output buffer
 * @param[in] out_length the length of the output buffer
 * @return whether the formatting was successful or not
 */
bool tostring128_signed(const uint128_t *const number,
                        uint32_t base,
                        char *const out,
                        uint32_t out_length) {
    uint128_t max_unsigned_val;
    uint128_t max_signed_val;
    uint128_t one_val;
    uint128_t two_val;
    uint128_t tmp;

    // showing negative numbers only really makes sense in base 10
    if (base == 10) {
        explicit_bzero(&one_val, sizeof(one_val));
        LOWER(one_val) = 1;
        explicit_bzero(&two_val, sizeof(two_val));
        LOWER(two_val) = 2;

        memset(&max_unsigned_val, 0xFF, sizeof(max_unsigned_val));
        divmod128(&max_unsigned_val, &two_val, &max_signed_val, &tmp);
        if (gt128(number, &max_signed_val))  // negative value
        {
            sub128(&max_unsigned_val, number, &tmp);
            add128(&tmp, &one_val, &tmp);
            out[0] = '-';
            return tostring128(&tmp, base, out + 1, out_length - 1);
        }
    }
    return tostring128(number, base, out, out_length);  // positive value
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
