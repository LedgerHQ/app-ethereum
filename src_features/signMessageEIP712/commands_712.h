#ifndef EIP712_H_
#define EIP712_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

#define DOMAIN_STRUCT_NAME "EIP712Domain"

bool handle_eip712_struct_def(const uint8_t *const apdu_buf);
bool handle_eip712_struct_impl(const uint8_t *const apdu_buf);
bool handle_eip712_sign(const uint8_t *const apdu_buf);
bool handle_eip712_filtering(const uint8_t *const apdu_buf);
void handle_eip712_return_code(bool success);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // EIP712_H_
