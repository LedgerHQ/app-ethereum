#ifndef MEM_UTILS_H_
#define MEM_UTILS_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include <stdbool.h>

#define MEM_ALLOC_AND_ALIGN_TYPE(type)  mem_alloc_and_align(sizeof(type), __alignof__(type))

char *mem_alloc_and_copy_char(char c);
void *mem_alloc_and_copy(const void *data, size_t size);
char *mem_alloc_and_format_uint(uint32_t value, uint8_t *const written_chars);
void *mem_alloc_and_align(size_t size, size_t alignment);

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // MEM_UTILS_H_
