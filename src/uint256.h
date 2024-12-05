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
#include "uint128.h"

typedef struct uint256_t {
    uint128_t elements[2];
} uint256_t;

void readu256BE(const uint8_t *const buffer, uint256_t *const target);
bool zero256(const uint256_t *const number);
void copy256(uint256_t *const target, const uint256_t *const number);
void clear256(uint256_t *const target);
void shiftl256(const uint256_t *const number, uint32_t value, uint256_t *const target);
void shiftr256(const uint256_t *const number, uint32_t value, uint256_t *const target);
uint32_t bits256(const uint256_t *const number);
bool equal256(const uint256_t *const number1, const uint256_t *const number2);
bool gt256(const uint256_t *const number1, const uint256_t *const number2);
bool gte256(const uint256_t *const number1, const uint256_t *const number2);
void add256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target);
void sub256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target);
void or256(const uint256_t *const number1, const uint256_t *const number2, uint256_t *const target);
bool mul256(const uint256_t *const number1,
            const uint256_t *const number2,
            uint256_t *const target);
void divmod256(const uint256_t *const l,
               const uint256_t *const r,
               uint256_t *const div,
               uint256_t *const mod);
bool tostring256(const uint256_t *const number, uint32_t base, char *const out, uint32_t outLength);
bool tostring256_signed(const uint256_t *const number,
                        uint32_t base,
                        char *const out,
                        uint32_t out_length);
void convertUint256BE(const uint8_t *const data, uint32_t length, uint256_t *const target);

#endif  // _UINT256_H_
