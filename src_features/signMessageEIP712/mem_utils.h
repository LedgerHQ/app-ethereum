#ifndef MEM_UTILS_H_
#define MEM_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

char    *alloc_and_copy_char(char c);
void    *alloc_and_copy(const void *data, size_t size);
uint8_t format_uint_into_mem(uint32_t value, const uint8_t max_chars);

#endif // MEM_UTILS_H_
