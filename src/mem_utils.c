#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mem.h"
#include "mem_utils.h"

/**
 * Format an unsigned number up to 32-bit into memory into an ASCII string.
 *
 * @param[in] value Value to write in memory
 * @return pointer to memory area or \ref NULL if the allocation failed
 */
const char *mem_alloc_and_format_uint(uint32_t value) {
    char *mem_ptr;
    uint32_t value_copy;
    uint8_t size;

    size = 1;  // minimum size, even if 0
    value_copy = value;
    while (value_copy >= 10) {
        value_copy /= 10;
        size += 1;
    }
    // +1 for the null character
    if ((mem_ptr = app_mem_alloc(sizeof(char) * (size + 1)))) {
        snprintf(mem_ptr, (size + 1), "%u", value);
    }
    return mem_ptr;
}

/**
 * Align memory by a given value
 *
 * @param[in] alignment given alignment value
 * @return size of the padding required for proper alignment
 */
uint8_t mem_legacy_align(size_t alignment) {
    uint8_t diff = (uintptr_t) mem_legacy_alloc(0) % alignment;

    if (diff > 0) {
        diff = alignment - diff;
        mem_legacy_alloc(diff);
    }
    return diff;
}

/**
 * Allocate and align, required when dealing with pointers of multi-bytes data
 * like structures that will be dereferenced at runtime.
 *
 * @param[in] size the size of the data we want to allocate in memory
 * @param[in] alignment the byte alignment needed
 *
 * @return pointer to the memory area, \ref NULL if the allocation failed
 */
void *mem_legacy_alloc_and_align(size_t size, size_t alignment) {
    mem_legacy_align(alignment);
    return mem_legacy_alloc(size);
}

char *app_mem_strdup(const char *src) {
    char *dst;
    size_t length = strlen(src) + 1;

    if ((dst = app_mem_alloc(length)) != NULL) {
        memcpy(dst, src, length);
    }
    return dst;
}
