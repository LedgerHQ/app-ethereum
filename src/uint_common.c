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

#include "uint_common.h"

void write_u64_be(uint8_t *const buffer, uint64_t value) {
    buffer[0] = ((value >> 56) & 0xff);
    buffer[1] = ((value >> 48) & 0xff);
    buffer[2] = ((value >> 40) & 0xff);
    buffer[3] = ((value >> 32) & 0xff);
    buffer[4] = ((value >> 24) & 0xff);
    buffer[5] = ((value >> 16) & 0xff);
    buffer[6] = ((value >> 8) & 0xff);
    buffer[7] = (value & 0xff);
}

void read_u64_be(const uint8_t *const in, uint64_t *const out) {
    uint8_t *out_ptr = (uint8_t *) out;
    *out_ptr++ = in[7];
    *out_ptr++ = in[6];
    *out_ptr++ = in[5];
    *out_ptr++ = in[4];
    *out_ptr++ = in[3];
    *out_ptr++ = in[2];
    *out_ptr++ = in[1];
    *out_ptr = in[0];
}

uint64_t readUint64BE(const uint8_t *const buffer) {
    return (((uint64_t) buffer[0]) << 56) | (((uint64_t) buffer[1]) << 48) |
           (((uint64_t) buffer[2]) << 40) | (((uint64_t) buffer[3]) << 32) |
           (((uint64_t) buffer[4]) << 24) | (((uint64_t) buffer[5]) << 16) |
           (((uint64_t) buffer[6]) << 8) | (((uint64_t) buffer[7]));
}

void reverseString(char *const str, uint32_t length) {
    uint32_t i, j;
    for (i = 0, j = length - 1; i < j; i++, j--) {
        char c;
        c = str[i];
        str[i] = str[j];
        str[j] = c;
    }
}
