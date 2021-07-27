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

#include <stdint.h>
#include <string.h>

#include "ethUstream.h"
#include "ethUtils.h"
#include "uint256.h"
#include "tokens.h"
#include "utils.h"

static const unsigned char hex_digits[] =
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

void array_hexstr(char *strbuf, const void *bin, unsigned int len) {
    while (len--) {
        *strbuf++ = hex_digits[((*((char *) bin)) >> 4) & 0xF];
        *strbuf++ = hex_digits[(*((char *) bin)) & 0xF];
        bin = (const void *) ((unsigned int) bin + 1);
    }
    *strbuf = 0;  // EOS
}

void convertUint256BE(uint8_t *data, uint32_t length, uint256_t *target) {
    uint8_t tmp[INT256_LENGTH];
    memset(tmp, 0, 32);
    memmove(tmp + 32 - length, data, length);
    readu256BE(tmp, target);
}

int local_strchr(char *string, char ch) {
    unsigned int length = strlen(string);
    unsigned int i;
    for (i = 0; i < length; i++) {
        if (string[i] == ch) {
            return i;
        }
    }
    return -1;
}

// Almost like U4BE except that it takes `size` as a parameter.
// The `strict` parameter defines whether we should throw in case of a length > 4.
uint32_t u32_from_BE(uint8_t *in, uint8_t size, bool strict) {
    switch (size) {
        case 0:
            return 0;
        case 1:
            return in[0];
        case 2:
            return (in[0] << 8) | in[1];
        case 3:
            return (in[0] << 16) | (in[1] << 8) | in[2];
        case 4:
            return (in[0] << 24) | (in[1] << 16) | (in[2] << 8) | in[3];
        default:
            if (strict) {
                PRINTF("Unexpected format\n");
                THROW(EXCEPTION);
            }
            return (in[0] << 24) | (in[1] << 16) | (in[2] << 8) | in[3];
    }
}

bool uint256_to_decimal(const uint8_t *value, size_t value_len, char *out, size_t out_len) {
    if (value_len > INT256_LENGTH) {
        // value len is bigger than INT256_LENGTH ?!
        return false;
    }

    uint16_t n[16] = {0};
    // Copy and right-align the number
    memcpy((uint8_t *) n + INT256_LENGTH - value_len, value, value_len);

    // Special case when value is 0
    if (allzeroes(n, INT256_LENGTH)) {
        if (out_len < 2) {
            // Not enough space to hold "0" and \0.
            return false;
        }
        strlcpy(out, "0", out_len);
        return true;
    }

    uint16_t *p = n;
    for (int i = 0; i < 16; i++) {
        n[i] = __builtin_bswap16(*p++);
    }
    int pos = out_len;
    while (!allzeroes(n, sizeof(n))) {
        if (pos == 0) {
            return false;
        }
        pos -= 1;
        int carry = 0;
        for (int i = 0; i < 16; i++) {
            int rem = ((carry << 16) | n[i]) % 10;
            n[i] = ((carry << 16) | n[i]) / 10;
            carry = rem;
        }
        out[pos] = '0' + carry;
    }
    memmove(out, out + pos, out_len - pos);
    out[out_len - pos] = 0;
    return true;
}

void amountToString(const uint8_t *amount,
                    uint8_t amount_size,
                    uint8_t decimals,
                    const char *ticker,
                    char *out_buffer,
                    uint8_t out_buffer_size) {
    char tmp_buffer[100] = {0};

    bool success = uint256_to_decimal(amount, amount_size, tmp_buffer, sizeof(tmp_buffer));

    if (!success) {
        THROW(0x6504);
    }

    uint8_t amount_len = strnlen(tmp_buffer, sizeof(tmp_buffer));
    uint8_t ticker_len = strnlen(ticker, MAX_TICKER_LEN);

    memcpy(out_buffer, ticker, MIN(out_buffer_size, ticker_len));

    adjustDecimals(tmp_buffer,
                   amount_len,
                   out_buffer + ticker_len,
                   out_buffer_size - ticker_len - 1,
                   decimals);
    out_buffer[out_buffer_size - 1] = '\0';
}

bool parse_swap_config(uint8_t *config, uint8_t config_len, char *ticker, uint8_t *decimals) {
    uint8_t ticker_len, offset = 0;
    if (config_len == 0) {
        return false;
    }
    ticker_len = config[offset++];
    if (ticker_len == 0 || ticker_len > MAX_TICKER_LEN - 2 || config_len - offset < ticker_len) {
        return false;
    }
    memcpy(ticker, config + offset, ticker_len);
    offset += ticker_len;
    ticker[ticker_len] = ' ';
    ticker[ticker_len + 1] = '\0';

    if (config_len - offset < 1) {
        return false;
    }
    *decimals = config[offset];
    return true;
}
