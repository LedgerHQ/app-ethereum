#pragma once

#include <stdint.h>
#include "eth_plugin_interface.h"

#define NUM_EIP7251_ADDRESSES 1
extern const uint8_t *const EIP7251_ADDRESSES[NUM_EIP7251_ADDRESSES];

void eip7251_plugin_call(eth_plugin_msg_t msg, void *parameters);
