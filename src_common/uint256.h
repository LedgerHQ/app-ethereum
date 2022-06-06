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

#ifndef _UINT256_H_
#define _UINT256_H_

#include <stdint.h>
#include <stdbool.h>
#include "os.h"
#include "cx.h"
#include "ethUstream.h"

typedef struct uint128_t {
    uint64_t elements[2];
} uint128_t;

typedef struct uint256_t {
    uint128_t elements[2];
} uint256_t;

#define UPPER_P(x) x->elements[0]
#define LOWER_P(x) x->elements[1]
#define UPPER(x)   x.elements[0]
#define LOWER(x)   x.elements[1]

void readu128BE(const uint8_t *const buffer, uint128_t *target);
void readu256BE(const uint8_t *const buffer, uint256_t *target);
void write_u64_be(uint8_t *const buffer, uint64_t value);
void read_u64_be(const uint8_t *const in, uint64_t *const out);
bool zero128(const uint128_t *const number);
bool zero256(const uint256_t *const number);
void copy128(uint128_t *const target, const uint128_t *const number);
void copy256(uint256_t *const target, const uint256_t *const number);
void clear128(uint128_t *const target);
void clear256(uint256_t *const target);
void shiftl128(const uint128_t *const number, uint32_t value, uint128_t *const target);
void shiftr128(const uint128_t *const number, uint32_t value, uint128_t *const target);
void shiftl256(const uint256_t *const number, uint32_t value, uint256_t *const target);
void shiftr256(const uint256_t *const number, uint32_t value, uint256_t *const target);
uint32_t bits128(const uint128_t *const number);
uint32_t bits256(const uint256_t *const number);
bool equal128(const uint128_t *const number1, const uint128_t *const number2);
bool equal256(const uint256_t *const number1, const uint256_t *const number2);
bool gt128(const uint128_t *const number1, const uint128_t *const number2);
bool gt256(const uint256_t *const number1, const uint256_t *const number2);
bool gte128(const uint128_t *const number1, const uint128_t *const number2);
bool gte256(const uint256_t *const number1, const uint256_t *const number2);
void add128(const uint128_t *const number1,
            const uint128_t *const number2,
            uint128_t *const target);
void add256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target);
void sub128(const uint128_t *const number1,
            const uint128_t *const number2,
            uint128_t *const target);
void sub256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target);
void or128(const uint128_t *const number1, const uint128_t *const number2, uint128_t *const target);
void or256(const uint256_t *const number1, const uint256_t *const number2, uint256_t *const target);
void mul128(const uint128_t *const number1,
            const uint128_t *const number2,
            uint128_t *const target);
void mul256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target);
void divmod128(const uint128_t *const l,
               const uint128_t *const r,
               uint128_t *const div,
               uint128_t *const mod);
void divmod256(const uint256_t *const l,
               const uint256_t *const r,
               uint256_t *const div,
               uint256_t *const mod);
bool tostring128(const uint128_t *const number, uint32_t base, char *const out, uint32_t outLength);
bool tostring256(const uint256_t *const number, uint32_t base, char *const out, uint32_t outLength);

#endif  // _UINT256_H_
