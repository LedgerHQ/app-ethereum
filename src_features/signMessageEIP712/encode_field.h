#ifndef ENCODE_FIELD_H_
#define ENCODE_FIELD_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include <stdbool.h>

#define EIP_712_ENCODED_FIELD_LENGTH 32

void *encode_uint(const uint8_t *const value, uint8_t length);
void *encode_int(const uint8_t *const value, uint8_t length, uint8_t typesize);
void *encode_boolean(const bool *const value, uint8_t length);
void *encode_address(const uint8_t *const value, uint8_t length);
void *encode_bytes(const uint8_t *const value, uint8_t length);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // ENCODE_FIELD_H_
