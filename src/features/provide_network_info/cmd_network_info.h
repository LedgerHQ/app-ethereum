#pragma once

#include <stdint.h>
#include "network.h"

uint16_t handle_network_info(uint8_t p1,
                             uint8_t p2,
                             const uint8_t *data,
                             uint8_t length,
                             unsigned int *tx);

void network_info_cleanup(network_info_t *network);
