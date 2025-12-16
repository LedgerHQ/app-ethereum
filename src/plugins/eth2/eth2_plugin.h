#pragma once

#include "common_utils.h"
#include "eth_plugin_interface.h"

#ifdef HAVE_ETH2

#define NUM_ETH2_SELECTORS 1
extern const uint8_t* const ETH2_SELECTORS[NUM_ETH2_SELECTORS];

#define NUM_ETH2_ADDRESSES 1
extern const uint8_t* const ETH2_ADDRESSES[NUM_ETH2_ADDRESSES];

void eth2_plugin_call(eth_plugin_msg_t message, void* parameters);

#endif
