#ifndef ENCODE_FIELD_H_
#define ENCODE_FIELD_H_

#include <stdint.h>
#include <stdbool.h>

#define EIP_712_ENCODED_FIELD_LENGTH    32

void    *encode_integer(const uint8_t *const value, uint8_t length);
void    *encode_boolean(const bool *const value, uint8_t length);
void    *encode_address(const uint8_t *const value, uint8_t length);

#endif // ENCODE_FIELD_H_
