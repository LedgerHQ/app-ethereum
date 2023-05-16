#ifndef _ETH_PLUGIN_INTERNAL_H_
#define _ETH_PLUGIN_INTERNAL_H_

#include <stdint.h>
#include <stdbool.h>
#include "eth_plugin_interface.h"

#define SELECTOR_SIZE    4
#define PARAMETER_LENGTH 32

void copy_address(uint8_t* dst, const uint8_t* parameter, uint8_t dst_size);

void copy_parameter(uint8_t* dst, const uint8_t* parameter, uint8_t dst_size);

void erc721_plugin_call(int message, void* parameters);
void erc1155_plugin_call(int message, void* parameters);

// Get the value from the beginning of the parameter (right to left) and check if the rest of it is
// zero
bool U2BE_from_parameter(const uint8_t* parameter, uint16_t* value);
bool U4BE_from_parameter(const uint8_t* parameter, uint32_t* value);

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

#ifdef HAVE_ETH2

#define NUM_ETH2_SELECTORS 1
extern const uint8_t* const ETH2_SELECTORS[NUM_ETH2_SELECTORS];

#endif

#ifdef HAVE_STARKWARE

#define NUM_STARKWARE_SELECTORS 20
extern const uint8_t* const STARKWARE_SELECTORS[NUM_STARKWARE_SELECTORS];

#endif

extern internalEthPlugin_t const INTERNAL_ETH_PLUGINS[];

#endif  // _ETH_PLUGIN_INTERNAL_H_
