#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DOMAIN_STRUCT_NAME "EIP712Domain"

uint16_t handle_eip712_struct_def(uint8_t p2, const uint8_t *cdata, uint8_t length);
uint16_t handle_eip712_struct_impl(uint8_t p1,
                                   uint8_t p2,
                                   const uint8_t *cdata,
                                   uint8_t length,
                                   uint32_t *flags);
uint16_t handle_eip712_sign(const uint8_t *cdata, uint8_t length, uint32_t *flags);
uint16_t handle_eip712_filtering(uint8_t p1,
                                 uint8_t p2,
                                 const uint8_t *cdata,
                                 uint8_t length,
                                 uint32_t *flags);
void handle_eip712_return_code(bool success);
