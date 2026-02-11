#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "chainConfig.h"
#include "nbgl_types.h"
#include "lists.h"

#define MAX_NETWORK_LEN 32  // 31 characters + '\0'

typedef struct network_info_s {
    flist_node_t node;  // MUST be first for direct casting from flist_node_t* to network_info_t*
    char name[MAX_NETWORK_LEN];
    char ticker[MAX_TICKER_LEN];
    uint64_t chain_id;
    nbgl_icon_details_t icon;
} network_info_t;

#define UNSUPPORTED_CHAIN_ID_MSG(chain_id)                                                  \
    do {                                                                                    \
        PRINTF("Unsupported chain ID: %llu (app: %llu)\n", chain_id, chainConfig->chainId); \
    } while (0)

extern const char g_unknown_ticker[];

/**
 * @brief Find a dynamically loaded network by its chain ID
 *
 * @param[in] chain_id The chain ID to search for
 * @return Pointer to network_info_t if found, NULL otherwise
 */
network_info_t *find_dynamic_network_by_chain_id(uint64_t chain_id);

const char *get_network_name_from_chain_id(const uint64_t *chain_id);
uint16_t get_network_as_string_from_chain_id(char *out, size_t out_size, uint64_t chain_id);
uint16_t get_network_as_string(char *out, size_t out_size);

bool chain_is_ethereum_compatible(const uint64_t *chain_id);
bool app_compatible_with_chain_id(const uint64_t *chain_id);

uint64_t get_tx_chain_id(void);

const char *get_displayable_ticker(const uint64_t *chain_id,
                                   const chain_config_t *chain_cfg,
                                   bool dynamic);
