#ifndef MEM_UTILS_H_
#define MEM_UTILS_H_

#ifdef HAVE_DYN_MEM_ALLOC

#include <stdint.h>
#include <stdbool.h>

#define MEM_ALLOC_AND_ALIGN_TYPE(type) mem_alloc_and_align(sizeof(type), __alignof__(type))

char *mem_alloc_and_format_uint(uint32_t value, uint8_t *const written_chars);
void *mem_alloc_and_align(size_t size, size_t alignment);

#endif  // HAVE_DYN_MEM_ALLOC

#endif  // MEM_UTILS_H_
