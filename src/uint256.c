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
#include "uint256.h"
#include "read.h"
#include "write.h"
#include "uint_common.h"
#include "common_utils.h"  // INT256_LENGTH

void readu256BE(const uint8_t *const buffer, uint256_t *const target) {
    readu128BE(buffer, &UPPER_P(target));
    readu128BE(buffer + 16, &LOWER_P(target));
}

bool zero256(const uint256_t *const number) {
    return (zero128(&LOWER_P(number)) && zero128(&UPPER_P(number)));
}

void copy256(uint256_t *const target, const uint256_t *const number) {
    copy128(&UPPER_P(target), &UPPER_P(number));
    copy128(&LOWER_P(target), &LOWER_P(number));
}

void clear256(uint256_t *const target) {
    clear128(&UPPER_P(target));
    clear128(&LOWER_P(target));
}

void shiftl256(const uint256_t *const number, uint32_t value, uint256_t *const target) {
    if (value >= 256) {
        clear256(target);
    } else if (value == 128) {
        copy128(&UPPER_P(target), &LOWER_P(number));
        clear128(&LOWER_P(target));
    } else if (value == 0) {
        copy256(target, number);
    } else if (value < 128) {
        uint128_t tmp1;
        uint128_t tmp2;
        uint256_t result;
        shiftl128(&UPPER_P(number), value, &tmp1);
        shiftr128(&LOWER_P(number), (128 - value), &tmp2);
        add128(&tmp1, &tmp2, &UPPER(result));
        shiftl128(&LOWER_P(number), value, &LOWER(result));
        copy256(target, &result);
    } else {
        shiftl128(&LOWER_P(number), (value - 128), &UPPER_P(target));
        clear128(&LOWER_P(target));
    }
}

void shiftr256(const uint256_t *const number, uint32_t value, uint256_t *const target) {
    if (value >= 256) {
        clear256(target);
    } else if (value == 128) {
        copy128(&LOWER_P(target), &UPPER_P(number));
        clear128(&UPPER_P(target));
    } else if (value == 0) {
        copy256(target, number);
    } else if (value < 128) {
        uint128_t tmp1;
        uint128_t tmp2;
        uint256_t result;
        shiftr128(&UPPER_P(number), value, &UPPER(result));
        shiftr128(&LOWER_P(number), value, &tmp1);
        shiftl128(&UPPER_P(number), (128 - value), &tmp2);
        add128(&tmp1, &tmp2, &LOWER(result));
        copy256(target, &result);
    } else {
        shiftr128(&UPPER_P(number), (value - 128), &LOWER_P(target));
        clear128(&UPPER_P(target));
    }
}

uint32_t bits256(const uint256_t *const number) {
    uint32_t result = 0;
    if (!zero128(&UPPER_P(number))) {
        result = 128;
        uint128_t up;
        copy128(&up, &UPPER_P(number));
        while (!zero128(&up)) {
            shiftr128(&up, 1, &up);
            result++;
        }
    } else {
        uint128_t low;
        copy128(&low, &LOWER_P(number));
        while (!zero128(&low)) {
            shiftr128(&low, 1, &low);
            result++;
        }
    }
    return result;
}

bool equal256(const uint256_t *const number1, const uint256_t *const number2) {
    return (equal128(&UPPER_P(number1), &UPPER_P(number2)) &&
            equal128(&LOWER_P(number1), &LOWER_P(number2)));
}

bool gt256(const uint256_t *const number1, const uint256_t *const number2) {
    if (equal128(&UPPER_P(number1), &UPPER_P(number2))) {
        return gt128(&LOWER_P(number1), &LOWER_P(number2));
    }
    return gt128(&UPPER_P(number1), &UPPER_P(number2));
}

bool gte256(const uint256_t *const number1, const uint256_t *const number2) {
    return gt256(number1, number2) || equal256(number1, number2);
}

void add256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target) {
    uint128_t tmp;
    add128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    add128(&LOWER_P(number1), &LOWER_P(number2), &tmp);
    if (gt128(&LOWER_P(number1), &tmp)) {
        uint128_t one;
        UPPER(one) = 0;
        LOWER(one) = 1;
        add128(&UPPER_P(target), &one, &UPPER_P(target));
    }
    add128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void sub256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target) {
    uint128_t tmp;
    sub128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    sub128(&LOWER_P(number1), &LOWER_P(number2), &tmp);
    if (gt128(&tmp, &LOWER_P(number1))) {
        uint128_t one;
        UPPER(one) = 0;
        LOWER(one) = 1;
        sub128(&UPPER_P(target), &one, &UPPER_P(target));
    }
    sub128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

void or256(const uint256_t *const number1,
           const uint256_t *const number2,
           uint256_t *const target) {
    or128(&UPPER_P(number1), &UPPER_P(number2), &UPPER_P(target));
    or128(&LOWER_P(number1), &LOWER_P(number2), &LOWER_P(target));
}

bool mul256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target) {
    uint8_t num1[INT256_LENGTH], num2[INT256_LENGTH], result[INT256_LENGTH * 2];
    memset(&result, 0, sizeof(result));
    for (uint8_t i = 0; i < 4; i++) {
        write_u64_be(num1 + i * sizeof(uint64_t), 0, number1->elements[i / 2].elements[i % 2]);
        write_u64_be(num2 + i * sizeof(uint64_t), 0, number2->elements[i / 2].elements[i % 2]);
    }
    if (cx_math_mult_no_throw(result, num1, num2, sizeof(num1)) != CX_OK) {
        return false;
    }
    for (uint8_t i = 0; i < 4; i++) {
        target->elements[i / 2].elements[i % 2] =
            read_u64_be((result + 32 + i * sizeof(uint64_t)), 0);
    }
    return true;
}

void divmod256(const uint256_t *const l,
               const uint256_t *const r,
               uint256_t *const retDiv,
               uint256_t *const retMod) {
    uint256_t copyd, adder, resDiv, resMod;
    uint256_t one;
    clear256(&one);
    UPPER(LOWER(one)) = 0;
    LOWER(LOWER(one)) = 1;
    uint32_t diffBits = bits256(l) - bits256(r);
    clear256(&resDiv);
    copy256(&resMod, l);
    if (gt256(r, l)) {
        copy256(retMod, l);
        clear256(retDiv);
    } else {
        shiftl256(r, diffBits, &copyd);
        shiftl256(&one, diffBits, &adder);
        if (gt256(&copyd, &resMod)) {
            shiftr256(&copyd, 1, &copyd);
            shiftr256(&adder, 1, &adder);
        }
        while (gte256(&resMod, r)) {
            if (gte256(&resMod, &copyd)) {
                sub256(&resMod, &copyd, &resMod);
                or256(&resDiv, &adder, &resDiv);
            }
            shiftr256(&copyd, 1, &copyd);
            shiftr256(&adder, 1, &adder);
        }
        copy256(retDiv, &resDiv);
        copy256(retMod, &resMod);
    }
}

bool tostring256(const uint256_t *const number,
                 uint32_t baseParam,
                 char *const out,
                 uint32_t outLength) {
    uint256_t rDiv;
    uint256_t rMod;
    uint256_t base;
    copy256(&rDiv, number);
    clear256(&rMod);
    clear256(&base);
    UPPER(LOWER(base)) = 0;
    LOWER(LOWER(base)) = baseParam;
    uint32_t offset = 0;
    if ((outLength == 0) || (baseParam < 2) || (baseParam > 16)) {
        return false;
    }
    do {
        divmod256(&rDiv, &base, &rDiv, &rMod);
        out[offset++] = HEXDIGITS[(uint8_t) LOWER(LOWER(rMod))];
    } while (!zero256(&rDiv) && (offset < outLength));

    if (offset == outLength) {  // destination buffer too small
        if (outLength > 3) {
            strlcpy(out, "...", outLength);
        } else {
            out[0] = '\0';
        }
        return false;
    }

    out[offset] = '\0';
    reverseString(out, offset);
    return true;
}

/**
 * Format a uint256_t into a string as a signed integer
 *
 * @param[in] number the number to format
 * @param[in] base the radix used in formatting
 * @param[out] out the output buffer
 * @param[in] out_length the length of the output buffer
 * @return whether the formatting was successful or not
 */
bool tostring256_signed(const uint256_t *const number,
                        uint32_t base,
                        char *const out,
                        uint32_t out_length) {
    uint256_t max_unsigned_val;
    uint256_t max_signed_val;
    uint256_t one_val;
    uint256_t two_val;
    uint256_t tmp;

    // showing negative numbers only really makes sense in base 10
    if (base == 10) {
        explicit_bzero(&one_val, sizeof(one_val));
        LOWER(LOWER(one_val)) = 1;
        explicit_bzero(&two_val, sizeof(two_val));
        LOWER(LOWER(two_val)) = 2;

        memset(&max_unsigned_val, 0xFF, sizeof(max_unsigned_val));
        divmod256(&max_unsigned_val, &two_val, &max_signed_val, &tmp);
        if (gt256(number, &max_signed_val))  // negative value
        {
            sub256(&max_unsigned_val, number, &tmp);
            add256(&tmp, &one_val, &tmp);
            out[0] = '-';
            return tostring256(&tmp, base, out + 1, out_length - 1);
        }
    }
    return tostring256(number, base, out, out_length);  // positive value
}

void convertUint256BE(const uint8_t *const data, uint32_t length, uint256_t *const target) {
    uint8_t tmp[INT256_LENGTH];

    memset(tmp, 0, sizeof(tmp) - length);
    memmove(tmp + sizeof(tmp) - length, data, length);
    readu256BE(tmp, target);
}
