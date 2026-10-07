#pragma once

#include <stdint.h>

uint16_t handle_eip712_v1_struct_def(uint8_t p2, const uint8_t *cdata, uint8_t length);
uint16_t handle_eip712_v1_struct_impl(uint8_t p1, uint8_t p2, const uint8_t *cdata, uint8_t length);
uint16_t handle_eip712_v1_sign(const uint8_t *cdata, uint8_t length);
uint16_t handle_eip712_v1_filtering(uint8_t p1, uint8_t p2, const uint8_t *cdata, uint8_t length);
