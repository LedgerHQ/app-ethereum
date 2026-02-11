#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "network.h"
#include "tlv_library.h"
#include "signature.h"

extern uint8_t *g_network_icon_hash;
extern flist_node_t *g_dynamic_network_list;
extern network_info_t *g_last_added_network;

bool handle_network_tlv_payload(const buffer_t *buf);
void network_info_cleanup(network_info_t *network);
