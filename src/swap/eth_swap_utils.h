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
#include <stdint.h>
#include "swap_lib_calls.h"
#include "chain_config.h"
#include "caller_app.h"

extern bool G_swap_checked;

// Chain ID the exchange application promised for the transaction being signed
extern uint64_t G_swap_expected_chain_id;

// Token contract the exchange application promised, when one was provided
extern uint8_t G_swap_expected_token_address[ADDRESS_LENGTH];
extern bool G_swap_has_expected_token_address;

// Whether the transaction carried a data field. Never cleared afterwards,
// unlike txContent.dataPresent which successful plugins clear for their UI.
extern bool G_swap_tx_had_calldata;

// Whether that calldata was checked against the exchange promise: the ERC-20
// swap path, and the crosschain path once the calldata hash matches.
extern bool G_swap_calldata_validated;

typedef struct {
    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;
} asset_info_t;

typedef struct {
    asset_info_t swapped_asset_info;
    asset_info_t fees_asset_info;
    uint64_t chain_id;
    uint8_t token_address[ADDRESS_LENGTH];
    bool has_token_address;
} swap_context_t;

typedef struct eth_libargs_s {
    unsigned int id;
    unsigned int command;
    chain_config_t *chain_config;
    union {
        check_address_parameters_t *check_address;
        create_transaction_parameters_t *create_transaction;
        get_printable_amount_parameters_t *get_printable_amount;
        caller_app_t *caller_app;
    };
} eth_libargs_t;

bool parse_swap_config(const uint8_t *config, uint8_t config_len, swap_context_t *context);
void get_asset_info_on_network(bool is_fee,
                               swap_context_t *context,
                               chain_config_t *config,
                               char **ticker,
                               uint8_t *decimals);

bool swap_check_destination(const char *destination);
bool swap_check_amount(const char *amount);
bool swap_check_value(const char *formatted_value, const uint8_t *raw_value, uint8_t raw_value_len);
bool swap_check_fee(const char *fee);

/**
 * @brief End the Exchange session when an error unwinds the signing context.
 *
 * Every swap guard is conditioned on G_called_from_swap, which the teardown clears.
 * Carrying on would run the rest of the session as an ordinary signing flow, with the
 * comparisons skipped and the double-sign safeguard disarmed. Does nothing outside a
 * swap session; never returns inside one.
 */
void swap_abort_session(void);
