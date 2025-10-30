#pragma once

#include <stdint.h>
#include <stdbool.h>

void roll_challenge(void);
uint32_t get_challenge(void);
bool check_challenge(uint32_t received_challenge);
uint16_t handle_get_challenge(unsigned int *tx);
