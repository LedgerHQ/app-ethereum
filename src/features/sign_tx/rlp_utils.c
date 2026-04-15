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

/**
 * @brief Utilities for an Ethereum Hardware Wallet logic
 * @file ethUtils.h
 * @author Ledger Firmware Team <hello@ledger.fr>
 * @version 1.0
 * @date 8th of March 2016
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "rlp_utils.h"

bool rlp_can_decode(uint8_t *buffer, uint32_t bufferLength, bool *valid) {
    if (*buffer <= RLP_SINGLE_BYTE_MAX) {
    } else if (*buffer <= RLP_SHORT_STRING_MAX) {
    } else if (*buffer <= RLP_LONG_STRING_MAX) {
        if (bufferLength < (1 + (*buffer - RLP_LONG_STRING_BASE))) {
            return false;
        }
        if (*buffer > RLP_STR_LEN_OF_BYTES_4) {
            *valid = false;  // arbitrary 32 bits length limitation
            return true;
        }
    } else if (*buffer <= RLP_SHORT_LIST_MAX) {
    } else {
        if (bufferLength < (1 + (*buffer - RLP_LONG_LIST_BASE))) {
            return false;
        }
        if (*buffer > RLP_LIST_LEN_OF_BYTES_4) {
            *valid = false;  // arbitrary 32 bits length limitation
            return true;
        }
    }
    *valid = true;
    return true;
}

bool rlp_decode_length(uint8_t *buffer, uint32_t *fieldLength, uint32_t *offset, bool *list) {
    if (*buffer <= RLP_SINGLE_BYTE_MAX) {
        *offset = 0;
        *fieldLength = 1;
        *list = false;
    } else if (*buffer <= RLP_SHORT_STRING_MAX) {
        *offset = 1;
        *fieldLength = *buffer - RLP_SHORT_STRING_BASE;
        *list = false;
    } else if (*buffer <= RLP_LONG_STRING_MAX) {
        *offset = 1 + (*buffer - RLP_LONG_STRING_BASE);
        *list = false;
        switch (*buffer) {
            case RLP_STR_LEN_OF_BYTES_1:
                *fieldLength = *(buffer + 1);
                break;
            case RLP_STR_LEN_OF_BYTES_2:
                *fieldLength = (*(buffer + 1) << 8) + *(buffer + 2);
                break;
            case RLP_STR_LEN_OF_BYTES_3:
                *fieldLength = (*(buffer + 1) << 16) + (*(buffer + 2) << 8) + *(buffer + 3);
                break;
            case RLP_STR_LEN_OF_BYTES_4:
                *fieldLength = (*(buffer + 1) << 24) + (*(buffer + 2) << 16) +
                               (*(buffer + 3) << 8) + *(buffer + 4);
                break;
            default:
                return false;  // arbitrary 32 bits length limitation
        }
    } else if (*buffer <= RLP_SHORT_LIST_MAX) {
        *offset = 1;
        *fieldLength = *buffer - RLP_SHORT_LIST_BASE;
        *list = true;
    } else {
        *offset = 1 + (*buffer - RLP_LONG_LIST_BASE);
        *list = true;
        switch (*buffer) {
            case RLP_LIST_LEN_OF_BYTES_1:
                *fieldLength = *(buffer + 1);
                break;
            case RLP_LIST_LEN_OF_BYTES_2:
                *fieldLength = (*(buffer + 1) << 8) + *(buffer + 2);
                break;
            case RLP_LIST_LEN_OF_BYTES_3:
                *fieldLength = (*(buffer + 1) << 16) + (*(buffer + 2) << 8) + *(buffer + 3);
                break;
            case RLP_LIST_LEN_OF_BYTES_4:
                *fieldLength = (*(buffer + 1) << 24) + (*(buffer + 2) << 16) +
                               (*(buffer + 3) << 8) + *(buffer + 4);
                break;
            default:
                return false;  // arbitrary 32 bits length limitation
        }
    }

    return true;
}
