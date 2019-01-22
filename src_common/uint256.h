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

typedef struct uint128_t { uint64_t elements[2]; } uint128_t;

typedef struct uint256_t { uint128_t elements[2]; } uint256_t;

#define UPPER_P(x) x->elements[0]
#define LOWER_P(x) x->elements[1]
#define UPPER(x) x.elements[0]
#define LOWER(x) x.elements[1]

void readu128BE(uint8_t *buffer, uint128_t *target);
void readu256BE(uint8_t *buffer, uint256_t *target);
bool zero128(uint128_t *number);
bool zero256(uint256_t *number);
void copy128(uint128_t *target, uint128_t *number);
void copy256(uint256_t *target, uint256_t *number);
void clear128(uint128_t *target);
void clear256(uint256_t *target);
void shiftl128(uint128_t *number, uint32_t value, uint128_t *target);
void shiftr128(uint128_t *number, uint32_t value, uint128_t *target);
void shiftl256(uint256_t *number, uint32_t value, uint256_t *target);
void shiftr256(uint256_t *number, uint32_t value, uint256_t *target);
uint32_t bits128(uint128_t *number);
uint32_t bits256(uint256_t *number);
bool equal128(uint128_t *number1, uint128_t *number2);
bool equal256(uint256_t *number1, uint256_t *number2);
bool gt128(uint128_t *number1, uint128_t *number2);
bool gt256(uint256_t *number1, uint256_t *number2);
bool gte128(uint128_t *number1, uint128_t *number2);
bool gte256(uint256_t *number1, uint256_t *number2);
void add128(uint128_t *number1, uint128_t *number2, uint128_t *target);
void add256(uint256_t *number1, uint256_t *number2, uint256_t *target);
void minus128(uint128_t *number1, uint128_t *number2, uint128_t *target);
void minus256(uint256_t *number1, uint256_t *number2, uint256_t *target);
void or128(uint128_t *number1, uint128_t *number2, uint128_t *target);
void or256(uint256_t *number1, uint256_t *number2, uint256_t *target);
void mul128(uint128_t *number1, uint128_t *number2, uint128_t *target);
void mul256(uint256_t *number1, uint256_t *number2, uint256_t *target);
void divmod128(uint128_t *l, uint128_t *r, uint128_t *div, uint128_t *mod);
void divmod256(uint256_t *l, uint256_t *r, uint256_t *div, uint256_t *mod);
bool tostring128(uint128_t *number, uint32_t base, char *out,
                 uint32_t outLength);
bool tostring256(uint256_t *number, uint32_t base, char *out,
                 uint32_t outLength);

#endif /* _UINT256_H_ */