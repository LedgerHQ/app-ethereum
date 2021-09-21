#ifndef __ETH_PLUGIN_INTERNAL_H__

#include "eth_plugin_interface.h"
#include "erc721_plugin.h"

#define SELECTOR_SIZE    4
#define PARAMETER_LENGTH 32
#define RUN_APPLICATION  1

typedef bool (*PluginAvailableCheck)(void);

typedef struct internalEthPlugin_t {
    PluginAvailableCheck availableCheck;
    const uint8_t** selectors;
    uint8_t num_selectors;
    char alias[10];
    PluginCall impl;
} internalEthPlugin_t;

#define NUM_ERC20_SELECTORS 2
extern const uint8_t* const ERC20_SELECTORS[NUM_ERC20_SELECTORS];

#define NUM_COMPOUND_SELECTORS 4
extern const uint8_t* const COMPOUND_SELECTORS[NUM_COMPOUND_SELECTORS];

#ifdef HAVE_ETH2

#define NUM_ETH2_SELECTORS 1
extern const uint8_t* const ETH2_SELECTORS[NUM_ETH2_SELECTORS];

#endif

#ifdef HAVE_STARKWARE

#define NUM_STARKWARE_SELECTORS 20
extern const uint8_t* const STARKWARE_SELECTORS[NUM_STARKWARE_SELECTORS];

#endif

extern internalEthPlugin_t const INTERNAL_ETH_PLUGINS[];

#endif
