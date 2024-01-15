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

#ifndef _UINT_COMMON_H_
#define _UINT_COMMON_H_

#include <stdint.h>

#define UPPER_P(x) x->elements[0]
#define LOWER_P(x) x->elements[1]
#define UPPER(x)   x.elements[0]
#define LOWER(x)   x.elements[1]

void write_u64_be(uint8_t *const buffer, uint64_t value);
void read_u64_be(const uint8_t *const in, uint64_t *const out);
uint64_t readUint64BE(const uint8_t *const buffer);
void reverseString(char *const str, uint32_t length);

#endif  //_UINT_COMMON_H_
