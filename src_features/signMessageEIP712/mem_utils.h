#ifndef MEM_UTILS_H_
#define MEM_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

#define MEM_ALLOC_AND_ALIGN_TO_TYPE(size, type) (mem_alloc_and_align(size, __alignof__(type)))

char *mem_alloc_and_copy_char(char c);
void *mem_alloc_and_copy(const void *data, size_t size);
char *mem_alloc_and_format_uint(uint32_t value, uint8_t *const written_chars);
void *mem_alloc_and_align(size_t size, size_t alignment);

#endif // MEM_UTILS_H_
