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

extern const uint8_t PARASWAP_ETHEREUM_ADDRESS[ADDRESS_LENGTH];

// Returns 1 if corresponding address is the Paraswap address for Ethereum (0xeeeee...).
#define ADDRESS_IS_ETH(_addr) (!memcmp(_addr, PARASWAP_ETHEREUM_ADDRESS, ADDRESS_LENGTH))

typedef enum {
    SWAP_ON_UNI,
    SWAP_ON_UNI_FORK,
    BUY_ON_UNI,
    BUY_ON_UNI_FORK,
} paraswapSelector_t;

typedef enum {
    AMOUNT_IN,
    AMOUNT_OUT,
    NUM_PATHS,
    TOKEN_SENT,
    TOKEN_RECEIVED,
} swap_params;

typedef struct paraswap_parameters_t {
    swap_params current_param;
    uint8_t skip;

    uint8_t amount_sent[BIGINT_LENGTH];  // corresponds to amountIn
    uint8_t contract_address_sent[ADDRESS_LENGTH];
    char ticker_sent[MAX_TICKER_LEN];

    uint8_t amount_received[BIGINT_LENGTH];  // corresponds to amountOutMin
    uint8_t contract_address_received[ADDRESS_LENGTH];
    char ticker_received[MAX_TICKER_LEN];


    uint8_t valid;
    uint8_t decimals_sent;
    uint8_t decimals_received;
    uint8_t selectorIndex;
    uint8_t num_paths;

} paraswap_parameters_t;

void handle_provide_parameter(void *parameters);