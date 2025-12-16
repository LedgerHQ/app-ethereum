#pragma once

#include "common_utils.h"

#ifdef HAVE_ETH2

#define NUM_ETH2_SELECTORS 1
extern const uint8_t* const ETH2_SELECTORS[NUM_ETH2_SELECTORS];

#define NUM_ETH2_ADDRESSES 1
extern const uint8_t* const ETH2_ADDRESSES[NUM_ETH2_ADDRESSES];

void eth2_plugin_call(int message, void* parameters);

#endif
