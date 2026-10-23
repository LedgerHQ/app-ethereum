#pragma once

#include <stdint.h>
#include "network.h"

uint16_t handle_network_icon_chunks(uint8_t p1, const buffer_t *buf);
void abort_icon_reception(void);
void clear_icon();
