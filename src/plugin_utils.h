#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SELECTOR_SIZE    4
#define PARAMETER_LENGTH 32

void copy_address(uint8_t* dst, const uint8_t* parameter, uint8_t dst_size);

void copy_parameter(uint8_t* dst, const uint8_t* parameter, uint8_t dst_size);

// Get the value from the beginning of the parameter (right to left) and check if the rest of it is
// zero
bool U2BE_from_parameter(const uint8_t* parameter, uint16_t* value);
bool U4BE_from_parameter(const uint8_t* parameter, uint32_t* value);
