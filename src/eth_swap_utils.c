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

#include <string.h>
#include <ctype.h>
#include "eth_swap_utils.h"
#include "asset_info.h"
#include "apdu_constants.h"
#include "utils.h"
#include "swap_error_code_helpers.h"
#include "feature_signTx.h"

bool G_swap_checked;

bool parse_swap_config(const uint8_t *config,
                       uint8_t config_len,
                       char *ticker,
                       uint8_t *decimals,
                       uint64_t *chain_id) {
    uint8_t ticker_len, offset = 0;

    if (config_len == 0) {
        return false;
    }
    ticker_len = config[offset];
    offset += sizeof(ticker_len);
    if ((ticker_len == 0) || (ticker_len > (MAX_TICKER_LEN - 2)) ||
        ((config_len - offset) < (ticker_len))) {
        return false;
    }
    memcpy(ticker, config + offset, ticker_len);
    offset += ticker_len;
    ticker[ticker_len] = '\0';

    if ((config_len - offset) < 1) {
        return false;
    }
    *decimals = config[offset];
    offset += sizeof(*decimals);

    // the chain ID was adder later to the CAL swap subconfig
    // so it is optional for retro-compatibility (as it might not be present)
    if ((config_len - offset) >= sizeof(*chain_id)) {
        PRINTF("Chain ID from the swap subconfig = 0x%.*h\n", sizeof(*chain_id), &config[offset]);
        *chain_id = u64_from_BE(config + offset, sizeof(*chain_id));
    }
    return true;
}

/* Local implementation of strncasecmp, workaround of the segfaulting base implem on return value
 * Remove once strncasecmp is fixed
 */
static int strcasecmp_workaround(const char *str1, const char *str2) {
    unsigned char c1, c2;
    do {
        c1 = *str1++;
        c2 = *str2++;
        if (toupper(c1) != toupper(c2)) {
            return toupper(c1) - toupper(c2);
        }
    } while (c1 != '\0');
    return 0;
}

bool swap_check_destination(const char *destination) {
    // Ensure the values are the same that the ones that have been previously validated
    if (strcasecmp_workaround(strings.common.toAddress, destination) != 0) {
        PRINTF("Error comparing destination addresses\n");
        send_swap_error_with_string(APDU_RESPONSE_MODE_CHECK_FAILED,
                                    SWAP_EC_ERROR_WRONG_DESTINATION,
                                    APP_CODE_DEFAULT,
                                    "%s != %s",
                                    strings.common.toAddress,
                                    destination);
        // unreachable
        os_sched_exit(0);
    }
    return true;
}

bool swap_check_amount(const char *amount) {
    // Ensure the values are the same that the ones that have been previously validated
    if (strcmp(strings.common.fullAmount, amount) != 0) {
        PRINTF("Error comparing amounts\n");
        send_swap_error_with_string(APDU_RESPONSE_MODE_CHECK_FAILED,
                                    SWAP_EC_ERROR_WRONG_AMOUNT,
                                    APP_CODE_DEFAULT,
                                    "%s != %s",
                                    strings.common.fullAmount,
                                    amount);
        // unreachable
        os_sched_exit(0);
    }
    return true;
}

bool swap_check_fee(const char *fee) {
    // Ensure the values are the same that the ones that have been previously validated
    if (strcmp(strings.common.maxFee, fee) != 0) {
        PRINTF("Error comparing fees\n");
        send_swap_error_with_string(APDU_RESPONSE_MODE_CHECK_FAILED,
                                    SWAP_EC_ERROR_WRONG_FEES,
                                    APP_CODE_DEFAULT,
                                    "%s != %s",
                                    strings.common.maxFee,
                                    fee);
        // unreachable
        os_sched_exit(0);
    }
    return true;
}
