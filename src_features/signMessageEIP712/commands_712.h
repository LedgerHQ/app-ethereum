#ifndef EIP712_H_
#define EIP712_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

// APDUs P1
#define P1_COMPLETE 0x00
#define P1_PARTIAL  0xFF

// APDUs P2
#define P2_DEF_NAME          0x00
#define P2_DEF_FIELD         0xFF
#define P2_IMPL_NAME         P2_DEF_NAME
#define P2_IMPL_ARRAY        0x0F
#define P2_IMPL_FIELD        P2_DEF_FIELD
#define P2_FILT_ACTIVATE     0x00
#define P2_FILT_MESSAGE_INFO 0x0F
#define P2_FILT_SHOW_FIELD   0xFF

#define DOMAIN_STRUCT_NAME "EIP712Domain"

bool handle_eip712_struct_def(const uint8_t *const apdu_buf);
bool handle_eip712_struct_impl(const uint8_t *const apdu_buf);
bool handle_eip712_sign(const uint8_t *const apdu_buf);
bool handle_eip712_filtering(const uint8_t *const apdu_buf);
void handle_eip712_return_code(bool success);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // EIP712_H_
