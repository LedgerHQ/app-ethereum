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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// RLP Encoding Constants (Ethereum RLP Specification)
// https://github.com/ethereum/wiki/wiki/RLP

// String encoding thresholds
#define RLP_SINGLE_BYTE_MAX  0x7f  // Single byte values [0x00, 0x7f]
#define RLP_SHORT_STRING_MAX 0xb7  // Short string [0x80, 0xb7]
#define RLP_LONG_STRING_MAX  0xbf  // Long string [0xb8, 0xbf]

// List encoding thresholds
#define RLP_SHORT_LIST_MAX 0xf7  // Short list [0xc0, 0xf7]
#define RLP_LONG_LIST_MAX  0xfb  // Long list [0xf8, 0xfb] (32-bit limitation)

// String length prefixes
#define RLP_STR_LEN_OF_BYTES_1 0xb8  // Length encoded in 1 byte
#define RLP_STR_LEN_OF_BYTES_2 0xb9  // Length encoded in 2 bytes
#define RLP_STR_LEN_OF_BYTES_3 0xba  // Length encoded in 3 bytes
#define RLP_STR_LEN_OF_BYTES_4 0xbb  // Length encoded in 4 bytes (max supported)

// List length prefixes
#define RLP_LIST_LEN_OF_BYTES_1 0xf8  // Length encoded in 1 byte
#define RLP_LIST_LEN_OF_BYTES_2 0xf9  // Length encoded in 2 bytes
#define RLP_LIST_LEN_OF_BYTES_3 0xfa  // Length encoded in 3 bytes
#define RLP_LIST_LEN_OF_BYTES_4 0xfb  // Length encoded in 4 bytes (max supported)

// Base offsets for length calculations
#define RLP_SHORT_STRING_BASE 0x80  // Base for short string length
#define RLP_LONG_STRING_BASE  0xb7  // Base for long string offset
#define RLP_SHORT_LIST_BASE   0xc0  // Base for short list length
#define RLP_LONG_LIST_BASE    0xf7  // Base for long list offset

/**
 * @brief Decode an RLP encoded field - see
 * https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] buffer buffer containing the RLP encoded field to decode
 * @param [out] fieldLength length of the RLP encoded field
 * @param [out] offset offset to the beginning of the RLP encoded field from the
 * buffer
 * @param [out] list true if the field encodes a list, false if it encodes a
 * string
 * @return true if the RLP header is consistent
 */
bool rlp_decode_length(uint8_t *buffer, uint32_t *fieldLength, uint32_t *offset, bool *list);

bool rlp_can_decode(uint8_t *buffer, uint32_t bufferLength, bool *valid);
