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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>

#include "uint256.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void array_hexstr(char* strbuf, const void* bin, unsigned int len);

void convertUint128BE(const uint8_t* const data, uint32_t length, uint128_t* const target);
void convertUint256BE(const uint8_t* const data, uint32_t length, uint256_t* const target);
void convertUint64BEto128(const uint8_t* const data, uint32_t length, uint128_t* const target);

uint64_t u64_from_BE(const uint8_t* in, uint8_t size);

bool uint256_to_decimal(const uint8_t* value, size_t value_len, char* out, size_t out_len);

void amountToString(const uint8_t* amount,
                    uint8_t amount_len,
                    uint8_t decimals,
                    const char* ticker,
                    char* out_buffer,
                    size_t out_buffer_size);

bool parse_swap_config(const uint8_t* config, uint8_t config_len, char* ticker, uint8_t* decimals);

#endif  // _UTILS_H_
