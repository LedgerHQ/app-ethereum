#ifndef EIP712_H_
#define EIP712_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

// APDUs P1
#define P1_COMPLETE 0x00
#define P1_PARTIAL  0xFF
#define P1_ACTIVATE         0x00
#define P1_CONTRACT_NAME    0x0F
#define P1_FIELD_NAME       0xFF

// APDUs P2
#define P2_NAME     0x00
#define P2_ARRAY    0x0F
#define P2_FIELD    0xFF
#define P2_KEY      0x00
#define P2_VALUE    0xFF

#define DOMAIN_STRUCT_NAME          "EIP712Domain"

bool    handle_eip712_struct_def(const uint8_t *const apdu_buf);
bool    handle_eip712_struct_impl(const uint8_t *const apdu_buf);
bool    handle_eip712_sign(const uint8_t *const apdu_buf);
bool    handle_eip712_filtering(const uint8_t *const apdu_buf);

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // EIP712_H_
