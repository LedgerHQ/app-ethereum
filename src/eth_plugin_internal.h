#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "shared_context.h"
#include "eth_plugin_interface.h"

void erc721_plugin_call(int message, void* parameters);
void erc1155_plugin_call(int message, void* parameters);
void swap_with_calldata_plugin_call(int message, void* parameters);

typedef bool (*const PluginAvailableCheck)(void);
typedef void (*PluginCall)(int, void*);

typedef struct internalEthPlugin_t {
    PluginAvailableCheck availableCheck;
    const uint8_t* const* selectors;
    uint8_t num_selectors;
    char alias[10];
    PluginCall impl;
} internalEthPlugin_t;

#define NUM_ERC20_SELECTORS 2
extern const uint8_t* const ERC20_SELECTORS[NUM_ERC20_SELECTORS];

#ifdef HAVE_ETH2

#define NUM_ETH2_SELECTORS 1
extern const uint8_t* const ETH2_SELECTORS[NUM_ETH2_SELECTORS];

#endif

extern internalEthPlugin_t const INTERNAL_ETH_PLUGINS[];
