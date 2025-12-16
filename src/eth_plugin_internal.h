#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "shared_context.h"
#include "eth_plugin_interface.h"

typedef void (*PluginCall)(eth_plugin_msg_t, void*);

typedef struct internalEthPlugin_t {
    const uint8_t* const* addresses;
    uint8_t num_addresses;
    const uint8_t* const* selectors;
    uint8_t num_selectors;
    char alias[10];
    PluginCall impl;
} internalEthPlugin_t;
