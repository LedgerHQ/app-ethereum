#pragma once

#include <stdint.h>
#include "network.h"

#define MAX_DYNAMIC_NETWORKS 2  // Nb configurations max to store

extern network_info_t DYNAMIC_NETWORK_INFO[];

uint16_t handleNetworkConfiguration(uint8_t p1,
                                    uint8_t p2,
                                    const uint8_t *data,
                                    uint8_t length,
                                    unsigned int *tx);
