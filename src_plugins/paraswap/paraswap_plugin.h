#pragma once

#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"
#include "withdrawal_index.h"

#define ETH2_DEPOSIT_PUBKEY_OFFSET         0x80
#define ETH2_WITHDRAWAL_CREDENTIALS_OFFSET 0xE0
#define ETH2_SIGNATURE_OFFSET              0x120
#define ETH2_DEPOSIT_PUBKEY_LENGTH         0x30
#define ETH2_WITHDRAWAL_CREDENTIALS_LENGTH 0x20
#define ETH2_SIGNATURE_LENGTH              0x60

extern const uint8_t PARASWAP_ETHEREUM_ADDRESS[20];

// Returns 1 if corresponding address is the Paraswap address for Ethereum (0xeeeee...).
#define ADDRESS_IS_ETH(_addr) (!memcmp(_addr, PARASWAP_ETHEREUM_ADDRESS, 20))

typedef enum {
    SWAP_ON_UNI,
    SWAP_ON_UNI_FORK,
} paraswapSelector_t;

typedef struct paraswap_parameters_t {
    uint8_t amount_sent[32];            // corresponds to amountIn
    uint8_t contract_address_sent[20];  // use constant
    char ticker_sent[MAX_TICKER_LEN];

    uint8_t amount_received[32];            // corresponds to amountOutMin
    uint8_t contract_address_received[20];  // use constant
    char ticker_received[MAX_TICKER_LEN];

    uint8_t valid;
    uint8_t decimals_sent;      // Not mandatory ?
    uint8_t decimals_received;  // Not mandatory ?
    uint8_t selectorIndex;
    uint8_t num_paths;
} paraswap_parameters_t;

void handle_provide_parameter(void *parameters);