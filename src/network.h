#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "chainConfig.h"

#define UNSUPPORTED_CHAIN_ID_MSG(id)                                              \
    do {                                                                          \
        PRINTF("Unsupported chain ID: %u (app: %u)\n", id, chainConfig->chainId); \
    } while (0)

const char *get_network_name_from_chain_id(const uint64_t *chain_id);
const char *get_network_ticker_from_chain_id(const uint64_t *chain_id);

bool chain_is_ethereum_compatible(const uint64_t *chain_id);
bool app_compatible_with_chain_id(const uint64_t *chain_id);

uint64_t get_tx_chain_id(void);

const char *get_displayable_ticker(const uint64_t *chain_id, const chain_config_t *chain_cfg);
