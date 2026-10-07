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

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct uint128_t {
    uint64_t elements[2];
} uint128_t;

void readu128BE(const uint8_t *buffer, uint128_t *target);
bool zero128(const uint128_t *number);
void copy128(uint128_t *target, const uint128_t *number);
void clear128(uint128_t *target);
void shiftl128(const uint128_t *number, uint32_t value, uint128_t *target);
void shiftr128(const uint128_t *number, uint32_t value, uint128_t *target);
uint32_t bits128(const uint128_t *number);
bool equal128(const uint128_t *number1, const uint128_t *number2);
bool gt128(const uint128_t *number1, const uint128_t *number2);
bool gte128(const uint128_t *number1, const uint128_t *number2);
void add128(const uint128_t *number1, const uint128_t *number2, uint128_t *target);
void sub128(const uint128_t *number1, const uint128_t *number2, uint128_t *target);
void or128(const uint128_t *number1, const uint128_t *number2, uint128_t *target);
bool mul128(const uint128_t *number1, const uint128_t *number2, uint128_t *target);
void divmod128(const uint128_t *l, const uint128_t *r, uint128_t *div, uint128_t *mod);
bool tostring128(const uint128_t *number, uint32_t base, char *out, uint32_t outLength);
bool tostring128_signed(const uint128_t *number, uint32_t base, char *out, uint32_t out_length);
void convertUint64BEto128(const uint8_t *data, uint32_t length, uint128_t *target);
void convertUint128BE(const uint8_t *data, uint32_t length, uint128_t *target);
