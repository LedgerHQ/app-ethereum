#pragma once

#include <stdbool.h>
#include <stdint.h>

uint16_t handle_sign_eip7702_authorization(uint8_t p1,
                                           const uint8_t *dataBuffer,
                                           uint8_t dataLength,
                                           unsigned int *flags);
