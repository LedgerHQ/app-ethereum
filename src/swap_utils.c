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

#include "ethUtils.h"
#include "uint128.h"
#include "uint256.h"
#include "tokens.h"
#include "swap_utils.h"

bool parse_swap_config(const uint8_t *config, uint8_t config_len, char *ticker, uint8_t *decimals) {
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
    ticker[ticker_len] = '\0';

    if (config_len - offset < 1) {
        return false;
    }
    *decimals = config[offset];
    return true;
}
