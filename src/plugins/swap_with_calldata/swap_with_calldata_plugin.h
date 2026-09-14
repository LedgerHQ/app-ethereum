#pragma once

#include "eth_plugin_interface.h"
#include "lcx_sha256.h"

typedef struct swap_with_calldata_context_s {
    // Hash of the calldata received.
    // Computed on (Selector + N * 32bytes parameters)
    // We don't care at all about the content, we'll just compare the hash with the promised hash.
    cx_sha256_t update_hash;
    uint32_t expected_size;
    uint32_t received_size;
} swap_with_calldata_context_t;

void swap_with_calldata_plugin_call(eth_plugin_msg_t message, void *parameters);
