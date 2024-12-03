#ifndef BUFFER_NORMALIZE_H_
#define BUFFER_NORMALIZE_H_

#include <stdint.h>

void buf_shrink_expand(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);
void str_cpy_explicit_trunc(const char *src, size_t src_size, char *dst, size_t dst_size);

#endif // BUFFER_NORMALIZE_H_
