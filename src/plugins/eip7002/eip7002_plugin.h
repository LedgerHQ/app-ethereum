#pragma once

#include <stdint.h>
#include "eth_plugin_interface.h"

#define NUM_EIP7002_ADDRESSES 1
extern const uint8_t *const EIP7002_ADDRESSES[NUM_EIP7002_ADDRESSES];

void eip7002_plugin_call(eth_plugin_msg_t msg, void *parameters);
