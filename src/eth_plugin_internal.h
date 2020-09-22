#ifndef __ETH_PLUGIN_INTERNAL_H__

#include "eth_plugin_interface.h"

#define SELECTOR_SIZE 4

typedef struct internalEthPlugin_t {
	const uint8_t **selectors;
	uint8_t num_selectors;
	char alias[7];
	PluginCall impl;
} internalEthPlugin_t;

#ifdef HAVE_STARKWARE

#define NUM_INTERNAL_PLUGINS 2

#define NUM_STARKWARE_SELECTORS 10

extern const uint8_t* const STARKWARE_SELECTORS[NUM_STARKWARE_SELECTORS];

#else

#define NUM_INTERNAL_PLUGINS 1

#endif

extern internalEthPlugin_t const INTERNAL_ETH_PLUGINS[NUM_INTERNAL_PLUGINS];

#endif
