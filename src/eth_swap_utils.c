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
#include "network.h"

// Global flag indicating whether swap parameters have been verified
bool G_swap_checked;

/**
 * Helper function to parse a token asset info (ticker + decimals) from config buffer
 *
 * @param config Pointer to the current position in config buffer
 * @param config_len Total length of the config buffer
 * @param info Output structure to store parsed token asset info
 * @param offset Pointer to current offset (updated after parsing)
 * @return true if parsing succeeds, false otherwise
 */
static bool parse_asset_info(const uint8_t *config,
                             uint8_t config_len,
                             asset_info_t *info,
                             uint8_t *offset) {
    uint8_t ticker_len;

    // Check if we have enough data for ticker length
    if (*offset >= config_len) {
        PRINTF("Not enough data for ticker length\n");
        return false;
    }

    // Read ticker length
    ticker_len = config[*offset];
    (*offset)++;

    // Validate ticker length
    if ((ticker_len == 0) || (ticker_len > (MAX_TICKER_LEN - 2))) {
        PRINTF("Ticker length is invalid (%d)\n", ticker_len);
        return false;
    }

    // Check if we have enough data for ticker
    if ((*offset + ticker_len) > config_len) {
        PRINTF("Not enough data for ticker\n");
        return false;
    }

    // Copy ticker and add null terminator
    memcpy(info->ticker, config + *offset, ticker_len);
    info->ticker[ticker_len] = '\0';
    *offset += ticker_len;

    // Check if we have enough data for decimals
    if (*offset >= config_len) {
        PRINTF("Not enough data for decimals\n");
        return false;
    }

    // Read decimals
    info->decimals = config[*offset];
    (*offset)++;

    return true;
}

/**
 * Parse swap configuration from CAL (Crypto Asset Library)
 *
 * @param config Buffer containing the configuration to parse
 * @param config_len Length of the configuration buffer
 * @param context Output structure to store all parsed swap information
 * @return true if parsing succeeds, false otherwise
 *
 * @note Configuration buffer format (app-specific):
 * @code
 * +---------+-------------------+----------+-------------------------+
 * | Offset  | Field             | Size     | Description             |
 * +---------+-------------------+----------+-------------------------+
 * | 0       | asset_ticker_len  | 1 byte   | Length of asset_ticker  |
 * | 1       | asset_ticker      | n bytes  | Asset ticker            |
 * | 1+n     | asset_decimals    | 1 byte   | Asset decimal places    |
 * | 2+n     | chain_id          | 8 bytes  | Chain ID (BE)           |
 * | 2+n+8   | fees_ticker_len   | 1 byte   | Length of fees_ticker   |
 * | 2+n+9   | fees_ticker       | m bytes  | Fees ticker             |
 * | 2+n+9+m | fees_decimals     | 1 byte   | Fees decimal places     |
 * +---------+-------------------+----------+-------------------------+
 * @endcode
 */
bool parse_swap_config(const uint8_t *config, uint8_t config_len, swap_context_t *context) {
    uint8_t offset = 0;
    uint8_t chainid_len = sizeof(context->chain_id);

    // Validate input parameters
    if ((config == NULL) || (config_len == 0) || (context == NULL)) {
        PRINTF("Invalid input parameters to parse_swap_config\n");
        return false;
    }

    // Initialize defaults
    explicit_bzero(context, sizeof(swap_context_t));
    context->fees_asset_info.decimals = WEI_TO_ETHER;  // Default to ETH decimals

    // Parse asset token info
    if (!parse_asset_info(config, config_len, &context->swapped_asset_info, &offset)) {
        PRINTF("Failed to parse swapped asset info\n");
        return false;
    }

    // Check if we have enough data for chain ID
    if ((config_len - offset) < chainid_len) {
        PRINTF("Failed to parse Chain ID\n");
        return false;
    }
    // Read chain ID (big-endian)
    PRINTF("Chain ID from the swap subconfig = 0x%.*h\n", chainid_len, &config[offset]);
    context->chain_id = u64_from_BE(config + offset, chainid_len);
    offset += chainid_len;

    // TODO: Remove this check once CAL is updated to always provide fees asset info
    // Parse fees asset info
    if (offset < config_len) {
        if (!parse_asset_info(config, config_len, &context->fees_asset_info, &offset)) {
            PRINTF("Failed to parse fees asset info\n");
            return false;
        }
        // Ensure the correct decimal is provided
        if (context->fees_asset_info.decimals != WEI_TO_ETHER) {
            PRINTF("Invalid fees decimals: %d\n", context->fees_asset_info.decimals);
            return false;
        }
    }

    return true;
}

/**
 * Get asset info (ticker and decimals) on the specified network
 *
 * @param is_fee Indicates if the amount is a fee
 * @param context Output structure to store all parsed swap information
 * @param config Chain configuration for fallback mechanisms
 * @param ticker Output pointer to store the resolved ticker
 * @param decimals Output pointer to store the resolved decimals (can be NULL)
 */
void get_asset_info_on_network(bool is_fee,
                               swap_context_t *context,
                               chain_config_t *config,
                               char **ticker,
                               uint8_t *decimals) {
    // If the amount is a fee, the ticker should be the chain's native currency
    if (is_fee) {
        // TODO: Remove this check once CAL is updated to always provide fees asset info
        if (context->fees_asset_info.ticker[0] == '\0') {
            // fallback mechanism in the absence of network ticker in swap config
            if (context->chain_id == 0) {
                // fallback mechanism in the absence of chain ID in swap config
                context->chain_id = config->chainId;
            }
            PRINTF("chain_id = %d\n", (uint32_t) context->chain_id);
            *ticker = (char *) get_displayable_ticker(&context->chain_id, config, false);
        } else {
            *ticker = context->fees_asset_info.ticker;
        }
        if (decimals != NULL) {
            *decimals = context->fees_asset_info.decimals;
        }
    } else {
        *ticker = context->swapped_asset_info.ticker;
        if (decimals != NULL) {
            *decimals = context->swapped_asset_info.decimals;
        }
    }
}

/**
 * Local implementation of strcasecmp, workaround for the segfaulting base implementation
 * on return value. Remove once strcasecmp is fixed.
 *
 * @param str1 First string to compare
 * @param str2 Second string to compare
 * @return 0 if strings are equal (case-insensitive), difference otherwise
 */
static int strcasecmp_workaround(const char *str1, const char *str2) {
    unsigned char c1, c2;

    LEDGER_ASSERT(str1 != NULL, "strcasecmp_workaround: str1 is NULL");
    LEDGER_ASSERT(str2 != NULL, "strcasecmp_workaround: str2 is NULL");
    do {
        c1 = *str1++;
        c2 = *str2++;
        if (toupper(c1) != toupper(c2)) {
            return toupper(c1) - toupper(c2);
        }
    } while (c1 != '\0');
    return 0;
}

/**
 * Verify that the destination address matches the previously validated one
 *
 * @param destination Destination address to verify
 * @return true if destination matches, false otherwise (exits app on failure)
 */
bool swap_check_destination(const char *destination) {
    if (destination == NULL) {
        return false;
    }
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
        return false;
    }
    return true;
}

/**
 * Verify that the amount matches the previously validated one
 *
 * @param amount Amount to verify
 * @return true if amount matches, false otherwise (exits app on failure)
 */
bool swap_check_amount(const char *amount) {
    if (amount == NULL) {
        return false;
    }
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
        return false;
    }
    return true;
}

/**
 * Verify that the fee matches the previously validated one
 *
 * @param fee Fee to verify
 * @return true if fee matches, false otherwise (exits app on failure)
 */
bool swap_check_fee(const char *fee) {
    if (fee == NULL) {
        return false;
    }
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
        return false;
    }
    return true;
}
