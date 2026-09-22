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
#include "main_std_app.h"  // app_exit
#include "utils.h"
#include "swap_error_code_helpers.h"
#include "feature_sign_tx.h"
#include "network.h"

// Global flag indicating whether swap parameters have been verified
bool G_swap_checked;
bool G_swap_tx_had_calldata;
bool G_swap_calldata_validated;

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
    // Cast to prevent any potential overflow issues
    if (((size_t) *offset + ticker_len) > config_len) {
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

    // TODO: Remove this check once CAL is updated to always provide the token contract address
    // Parse token contract address (ERC-20 swaps only)
    if (offset < config_len) {
        if ((config_len - offset) < ADDRESS_LENGTH) {
            PRINTF("Failed to parse token contract address\n");
            return false;
        }
        memcpy(context->token_address, config + offset, ADDRESS_LENGTH);
        context->has_token_address = true;
        offset += ADDRESS_LENGTH;
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
                context->chain_id = config->chain_id;
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
    if (strcasecmp(strings.common.toAddress, destination) != 0) {
        PRINTF("Error comparing destination addresses\n");
        send_swap_error_with_string(APDU_RESPONSE_MODE_CHECK_FAILED,
                                    SWAP_EC_ERROR_WRONG_DESTINATION,
                                    APP_CODE_DEFAULT,
                                    "%s != %s",
                                    strings.common.toAddress,
                                    destination);
        // unreachable — app_exit is __attribute__((noreturn))
        app_exit();
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
        // unreachable — app_exit is __attribute__((noreturn))
        app_exit();
    }
    return true;
}

/**
 * Verify that the transaction's value field holds what was validated in Exchange
 *
 * What that is depends on the asset being swapped, as decided by copy_transaction_parameters():
 * the validated amount when the native currency is spent, zero when a token is spent (see
 * swap_value_check_t). Neither form tolerates anything else: this value is the only part of the
 * spend the user validated in Exchange that the app can bind on its own.
 *
 * Both forms are checked on the raw value rather than on its formatted form. For the zero case,
 * the expected "0 <native ticker>" string is built from the swap config's fee asset info while the
 * observed one is built from the network the transaction declares, so comparing them would make
 * the signature depend on those two tickers spelling the native currency identically. For the
 * amount case, the formatted forms of two amounts on different scales collide outright: an asset
 * declaring 6 decimals makes 1 token and 1 native unit both read "1 <ticker>". The chain ID is
 * bound separately anyway, in finalize_parsing_helper().
 *
 * @param formatted_value Transaction value, formatted the way Exchange formats amounts
 * @param raw_value Transaction value, as parsed from the transaction
 * @param raw_value_len Length of raw_value in bytes
 * @return true if the value matches, false otherwise (exits app on failure)
 */
bool swap_check_value(const char *formatted_value,
                      const uint8_t *raw_value,
                      uint8_t raw_value_len) {
    uint8_t observed_value[INT256_LENGTH];

    if ((formatted_value == NULL) || (raw_value == NULL)) {
        return false;
    }
    if (raw_value_len > sizeof(observed_value)) {
        // Cannot happen: the RLP parser caps the value at a word. Fail closed rather than skip
        // the check, since the only caller does not read this function's return value.
        PRINTF("Error: transaction value longer than a word\n");
        send_swap_error_simple(APDU_RESPONSE_MODE_CHECK_FAILED,
                               SWAP_EC_ERROR_WRONG_AMOUNT,
                               APP_CODE_DEFAULT);
        // unreachable
        os_sched_exit(0);
        return false;
    }
    bool value_is_zero = (allzeroes(raw_value, raw_value_len) == 1);

    // Right-align the value in a full word, the way G_swap_expected_value is staged, so the two
    // can be compared whatever minimal length each side was encoded with.
    explicit_bzero(observed_value, sizeof(observed_value));
    memcpy(observed_value + sizeof(observed_value) - raw_value_len, raw_value, raw_value_len);

    switch (G_swap_expected_value_check) {
        case SWAP_VALUE_CHECK_ZERO:
            if (!value_is_zero) {
                PRINTF("Error: non-zero value for a token swap\n");
                send_swap_error_with_string(APDU_RESPONSE_MODE_CHECK_FAILED,
                                            SWAP_EC_ERROR_WRONG_AMOUNT,
                                            APP_CODE_DEFAULT,
                                            "expected a zero value, got %s",
                                            formatted_value);
                // unreachable
                os_sched_exit(0);
                return false;
            }
            return true;
        case SWAP_VALUE_CHECK_AMOUNT:
        default:
            if (memcmp(observed_value, G_swap_expected_value, sizeof(observed_value)) != 0) {
                PRINTF("Error: value differs from the validated amount\n");
                // Both sides are printed because they can read identically: that two equal
                // strings can stand for different amounts is what this check exists for
                send_swap_error_with_string(APDU_RESPONSE_MODE_CHECK_FAILED,
                                            SWAP_EC_ERROR_WRONG_AMOUNT,
                                            APP_CODE_DEFAULT,
                                            "raw value mismatch: expected %s, got %s",
                                            strings.common.fullAmount,
                                            formatted_value);
                // unreachable
                os_sched_exit(0);
                return false;
            }
            // The raw bytes match; the formatted comparison additionally binds the ticker the two
            // sides spell the asset with.
            return swap_check_amount(formatted_value);
    }
}

/**
 * Verify that the fee matches the previously validated one
 *
 * @param fee Fee to verify
 * @return true if fee matches, false otherwise (exits app on failure)
 */
void swap_abort_session(void) {
    if (!G_called_from_swap) {
        return;
    }
    PRINTF("Error inside a swap session: ending it\n");
    G_swap_response_ready = true;
    send_swap_error_simple(APDU_RESPONSE_MODE_CHECK_FAILED,
                           SWAP_EC_ERROR_GENERIC,
                           APP_CODE_DEFAULT);
}

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
        // unreachable — app_exit is __attribute__((noreturn))
        app_exit();
    }
    return true;
}
