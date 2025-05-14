#pragma once

#include <stdint.h>

void roll_challenge(void);
uint32_t get_challenge(void);
uint16_t handle_get_challenge(unsigned int *tx);
