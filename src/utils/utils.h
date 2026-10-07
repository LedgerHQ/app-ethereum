#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SET_BIT(a) (1 << a)

void buf_shrink_expand(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);
void str_cpy_explicit_trunc(const char *src, size_t src_size, char *dst, size_t dst_size);
void reverseString(char *const str, uint32_t length);
bool format_signed_int_be(const uint8_t *data,
                          size_t length,
                          uint8_t type_size,
                          char *buf,
                          size_t buf_size);
