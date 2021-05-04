#pragma once

#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"
#include "withdrawal_index.h"

#define ADDRESS_LENGTH   20
#define PARAMETER_LENGTH 32
#define BIGINT_LENGTH    32
#define SELECTOR_SIZE    4

// Paraswap uses `0xeeeee` as a dummy address to represent ETH.
extern const uint8_t PARASWAP_ETH_ADDRESS[ADDRESS_LENGTH];

// Returns 1 if corresponding address is the Paraswap address for ETH (0xeeeee...).
#define ADDRESS_IS_ETH(_addr) (!memcmp(_addr, PARASWAP_ETH_ADDRESS, ADDRESS_LENGTH))

typedef enum {
    SWAP_ON_UNI,
    SWAP_ON_UNI_FORK,
    BUY_ON_UNI,
    BUY_ON_UNI_FORK,
    SIMPLE_SWAP,
    SIMPLE_BUY,
    MULTI_SWAP,
    BUY,
    MEGA_SWAP,
} paraswapSelector_t;

typedef enum {
    AMOUNT_SENT,      // Amount sent by the user to the contract.
    AMOUNT_RECEIVED,  // Amount sent by the contract to the user.
    TOKEN_SENT,       // Address of the token the user is sending.
    TOKEN_RECEIVED,   // Address of the token sent to the user.
    PATH,  // Path of the different asseths that will get swapped during the trade. First and last
           // tokens are the ones we care about.
    EXPECTED_AMOUNT,  // Expected amount SCOTT
    CALLEES,
    EXCHANGE_DATA,
    START_INDEXES,
    VALUES,
    BENEFICIARY, // Address to which the contract will send the tokens.
} swap_params;

typedef struct paraswap_parameters_t {
    uint8_t amount_sent[BIGINT_LENGTH];
    uint8_t amount_received[BIGINT_LENGTH];
    char beneficiary[ADDRESS_LENGTH];
    uint8_t contract_address_sent[ADDRESS_LENGTH];
    uint8_t contract_address_received[ADDRESS_LENGTH];
    char ticker_sent[MAX_TICKER_LEN];
    char ticker_received[MAX_TICKER_LEN];

    swap_params next_param;
    uint8_t valid;
    uint8_t decimals_sent;
    uint8_t decimals_received;
    uint8_t selectorIndex;
    uint8_t list_len;
    uint8_t skip;

} paraswap_parameters_t;

void handle_provide_parameter(void *parameters);