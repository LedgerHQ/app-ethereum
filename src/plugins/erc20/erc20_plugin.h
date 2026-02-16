#pragma once

#include "eth_plugin_interface.h"

#define NUM_ERC20_SELECTORS 2
extern const uint8_t* const ERC20_SELECTORS[NUM_ERC20_SELECTORS];

void erc20_plugin_call(eth_plugin_msg_t message, void* parameters);
