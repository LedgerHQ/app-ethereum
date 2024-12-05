#ifndef SOL_TYPENAMES_H_
#define SOL_TYPENAMES_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

bool sol_typenames_init(void);

const char *get_struct_field_sol_typename(const uint8_t *ptr, uint8_t *const length);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // SOL_TYPENAMES_H_
