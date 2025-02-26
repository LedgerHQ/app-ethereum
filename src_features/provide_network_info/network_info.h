#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "network.h"
#include "tlv.h"

#define MAX_DYNAMIC_NETWORKS 2  // Nb configurations max to store

// Signature context structure
typedef struct {
    network_info_t network;
    uint8_t icon_hash[CX_SHA256_SIZE];
    uint8_t signature_length;
    uint8_t signature[73];
    cx_sha256_t hash_ctx;
} s_network_info_ctx;

#ifdef HAVE_NBGL
extern uint8_t g_network_icon_hash[MAX_DYNAMIC_NETWORKS][CX_SHA256_SIZE];
#endif
extern network_info_t DYNAMIC_NETWORK_INFO[MAX_DYNAMIC_NETWORKS];
extern uint8_t g_current_network_slot;

bool handle_network_info_struct(const s_tlv_data *data, s_network_info_ctx *context);
bool verify_network_info_struct(const s_network_info_ctx *context);
