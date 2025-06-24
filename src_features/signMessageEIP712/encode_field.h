#pragma once

#include <stdint.h>
#include <stdbool.h>

#define EIP_712_ENCODED_FIELD_LENGTH 32

void *encode_uint(const uint8_t *value, uint8_t length);
void *encode_int(const uint8_t *value, uint8_t length, uint8_t typesize);
void *encode_boolean(const bool *value, uint8_t length);
void *encode_address(const uint8_t *value, uint8_t length);
void *encode_bytes(const uint8_t *value, uint8_t length);
