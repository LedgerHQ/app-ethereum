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
#include "lcx_math.h"  // cx_math_mult_no_throw
#include "os_utils.h"  // is_zeroes_buffer
#include "read.h"
#include "write.h"
#include "uint128.h"
#include "uint_common.h"
#include "common_utils.h"  // HEXDIGITS, INT128_LENGTH
#include "utils.h"

void readu128BE(const uint8_t *buffer, uint128_t *target) {
    UPPER_P(target) = read_u64_be(buffer, 0);
    LOWER_P(target) = read_u64_be(buffer + 8, 0);
}

bool zero128(const uint128_t *number) {
    return ((LOWER_P(number) == 0) && (UPPER_P(number) == 0));
}

void copy128(uint128_t *target, const uint128_t *number) {
    UPPER_P(target) = UPPER_P(number);
    LOWER_P(target) = LOWER_P(number);
}

void clear128(uint128_t *target) {
    UPPER_P(target) = 0;
    LOWER_P(target) = 0;
}

void shiftl128(const uint128_t *number, uint32_t value, uint128_t *target) {
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

void shiftr128(const uint128_t *number, uint32_t value, uint128_t *target) {
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

uint32_t bits128(const uint128_t *number) {
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

bool equal128(const uint128_t *number1, const uint128_t *number2) {
    return (UPPER_P(number1) == UPPER_P(number2)) && (LOWER_P(number1) == LOWER_P(number2));
}

bool gt128(const uint128_t *number1, const uint128_t *number2) {
    if (UPPER_P(number1) == UPPER_P(number2)) {
        return (LOWER_P(number1) > LOWER_P(number2));
    }
    return (UPPER_P(number1) > UPPER_P(number2));
}

bool gte128(const uint128_t *number1, const uint128_t *number2) {
    return gt128(number1, number2) || equal128(number1, number2);
}

void add128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) = UPPER_P(number1) + UPPER_P(number2) +
                      ((LOWER_P(number1) + LOWER_P(number2)) < LOWER_P(number1));
    LOWER_P(target) = LOWER_P(number1) + LOWER_P(number2);
}

void sub128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) = UPPER_P(number1) - UPPER_P(number2) -
                      ((LOWER_P(number1) - LOWER_P(number2)) > LOWER_P(number1));
    LOWER_P(target) = LOWER_P(number1) - LOWER_P(number2);
}

void or128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    UPPER_P(target) = UPPER_P(number1) | UPPER_P(number2);
    LOWER_P(target) = LOWER_P(number1) | LOWER_P(number2);
}

bool mul128(const uint128_t *number1, const uint128_t *number2, uint128_t *target) {
    // Match mul256's structure: feed two 16-byte big-endian operands
    // into the SDK syscall, which produces a 32-byte big-endian product
    // (bytes 0..15 = high 128 bits, bytes 16..31 = low 128 bits that we
    // keep). Any nonzero byte in the high half means the product does
    // not fit in uint128 — return false so callers cannot silently
    // truncate.
    //
    // The previous schoolbook implementation returned void and threw
    // the overflow carries away inside `>> 32` operations, leaving any
    // future caller exposed to the same display-truth gap mul256 had
    // (CWE-682). No production code calls mul128 today; the rewrite is
    // a defensive hardening so the first such caller cannot trip it.
    uint8_t num1[INT128_LENGTH], num2[INT128_LENGTH];
    uint8_t result[INT128_LENGTH * 2];
    memset(result, 0, sizeof(result));
    write_u64_be(num1, 0, UPPER_P(number1));
    write_u64_be(num1 + sizeof(uint64_t), 0, LOWER_P(number1));
    write_u64_be(num2, 0, UPPER_P(number2));
    write_u64_be(num2 + sizeof(uint64_t), 0, LOWER_P(number2));
    if (cx_math_mult_no_throw(result, num1, num2, sizeof(num1)) != CX_OK) {
        return false;
    }
    if (!is_zeroes_buffer(result, INT128_LENGTH)) {
        return false;
    }
    UPPER_P(target) = read_u64_be(result + INT128_LENGTH, 0);
    LOWER_P(target) = read_u64_be(result + INT128_LENGTH + sizeof(uint64_t), 0);
    return true;
}

void divmod128(const uint128_t *l, const uint128_t *r, uint128_t *retDiv, uint128_t *retMod) {
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

bool tostring128(const uint128_t *number, uint32_t baseParam, char *out, uint32_t outLength) {
    uint128_t rDiv;
    uint128_t rMod;
    uint128_t base;
    copy128(&rDiv, number);
    clear128(&rMod);
    clear128(&base);
    LOWER(base) = baseParam;
    uint32_t offset = 0;
    if ((outLength == 0) || (baseParam < 2) || (baseParam > 16)) {
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
bool tostring128_signed(const uint128_t *number, uint32_t base, char *out, uint32_t out_length) {
    uint128_t max_unsigned_val;
    uint128_t max_signed_val;
    uint128_t one_val;
    uint128_t two_val;
    uint128_t tmp;

    if ((out == NULL) || (out_length == 0)) {
        return false;
    }

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
            if (out_length < 2) {
                out[0] = '\0';
                return false;
            }
            sub128(&max_unsigned_val, number, &tmp);
            add128(&tmp, &one_val, &tmp);
            out[0] = '-';
            return tostring128(&tmp, base, out + 1, out_length - 1);
        }
    }
    return tostring128(number, base, out, out_length);  // positive value
}

void convertUint64BEto128(const uint8_t *data, uint32_t length, uint128_t *target) {
    uint8_t tmp[INT128_LENGTH];
    int64_t value;

    value = u64_from_BE(data, length);
    if (length > sizeof(tmp)) {
        memset(tmp, 0, sizeof(tmp));
        return;
    }
    memset(tmp, ((value < 0) ? 0xff : 0), sizeof(tmp) - length);
    memmove(tmp + sizeof(tmp) - length, data, length);
    readu128BE(tmp, target);
}

void convertUint128BE(const uint8_t *data, uint32_t length, uint128_t *target) {
    uint8_t tmp[INT128_LENGTH];

    if (data == NULL || target == NULL || length == 0) {
        return;
    }
    if (length > sizeof(tmp)) {
        return;
    }

    memset(tmp, 0, sizeof(tmp) - length);
    memmove(tmp + sizeof(tmp) - length, data, length);
    readu128BE(tmp, target);
}
