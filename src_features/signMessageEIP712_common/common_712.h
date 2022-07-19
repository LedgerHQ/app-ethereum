#ifndef COMMON_EIP712_H_
#define COMMON_EIP712_H_

#include <stdint.h>
#include "ux.h"

unsigned int ui_712_approve_cb(const bagl_element_t *e);
unsigned int ui_712_reject_cb(const bagl_element_t *e);

#endif  // COMMON_EIP712_H_
