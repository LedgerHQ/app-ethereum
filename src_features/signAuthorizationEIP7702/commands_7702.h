#pragma once

#include <stdbool.h>
#include <stdint.h>

uint16_t handleSignEIP7702Authorization(uint8_t p1,
                                        const uint8_t *dataBuffer,
                                        uint8_t dataLength,
                                        unsigned int *flags);
