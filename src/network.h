#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "chainConfig.h"
#ifdef HAVE_NBGL
#include "nbgl_types.h"
#endif

#define MAX_NETWORK_LEN 32  // 31 characters + '\0'

typedef struct network_info_s {
    const char name[MAX_NETWORK_LEN];
    const char ticker[MAX_TICKER_LEN];
    uint64_t chain_id;
#ifdef HAVE_NBGL
    nbgl_icon_details_t icon;
#endif
} network_info_t;

#define UNSUPPORTED_CHAIN_ID_MSG(id)                                              \
    do {                                                                          \
        PRINTF("Unsupported chain ID: %u (app: %u)\n", id, chainConfig->chainId); \
    } while (0)

extern network_info_t DYNAMIC_NETWORK_INFO[];
extern const char g_unknown_ticker[];

const char *get_network_name_from_chain_id(const uint64_t *chain_id);
const char *get_network_ticker_from_chain_id(const uint64_t *chain_id);

bool chain_is_ethereum_compatible(const uint64_t *chain_id);
bool app_compatible_with_chain_id(const uint64_t *chain_id);

uint64_t get_tx_chain_id(void);

const char *get_displayable_ticker(const uint64_t *chain_id, const chain_config_t *chain_cfg);
