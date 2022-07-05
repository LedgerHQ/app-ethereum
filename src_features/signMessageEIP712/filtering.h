#ifndef FILTERING_H_
#define FILTERING_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

bool    provide_filtering_info(const uint8_t *const payload, uint8_t length, uint8_t p1);

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // FILTERING_H_
