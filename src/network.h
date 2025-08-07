#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "chainConfig.h"
#include "nbgl_types.h"

#define MAX_NETWORK_LEN 32  // 31 characters + '\0'

typedef struct network_info_s {
    char name[MAX_NETWORK_LEN];
    char ticker[MAX_TICKER_LEN];
    uint64_t chain_id;
    nbgl_icon_details_t icon;
} network_info_t;

#define UNSUPPORTED_CHAIN_ID_MSG(id)                                              \
    do {                                                                          \
        PRINTF("Unsupported chain ID: %u (app: %u)\n", id, chainConfig->chainId); \
    } while (0)

extern const char g_unknown_ticker[];

const char *get_network_name_from_chain_id(const uint64_t *chain_id);
uint16_t get_network_as_string(char *out, size_t out_size);
const char *get_network_ticker_from_chain_id(const uint64_t *chain_id);

bool chain_is_ethereum_compatible(const uint64_t *chain_id);
bool app_compatible_with_chain_id(const uint64_t *chain_id);

uint64_t get_tx_chain_id(void);

const char *get_displayable_ticker(const uint64_t *chain_id, const chain_config_t *chain_cfg);
