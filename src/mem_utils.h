#pragma once

#include <stdint.h>

#define MEM_ALLOC_AND_ALIGN_TYPE(type) mem_legacy_alloc_and_align(sizeof(type), __alignof__(type))

char *mem_legacy_alloc_and_format_uint(uint32_t value, uint8_t *const written_chars);
uint8_t mem_legacy_align(size_t alignment);
void *mem_legacy_alloc_and_align(size_t size, size_t alignment);
