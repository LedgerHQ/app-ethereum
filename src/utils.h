#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>

#define SET_BIT(a) (1 << a)

void buf_shrink_expand(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);
void str_cpy_explicit_trunc(const char *src, size_t src_size, char *dst, size_t dst_size);

#endif  // !UTILS_H_
