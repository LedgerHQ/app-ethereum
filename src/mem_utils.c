#ifdef HAVE_DYN_MEM_ALLOC

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mem.h"
#include "mem_utils.h"

/**
 * Format an unsigned number up to 32-bit into memory into an ASCII string.
 *
 * @param[in] value Value to write in memory
 * @param[out] length number of characters written to memory
 *
 * @return pointer to memory area or \ref NULL if the allocation failed
 */
char *mem_alloc_and_format_uint(uint32_t value, uint8_t *const length) {
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
    if ((mem_ptr = mem_alloc(sizeof(char) * (size + 1)))) {
        snprintf(mem_ptr, (size + 1), "%u", value);
        mem_dealloc(sizeof(char));  // to skip the null character
        if (length != NULL) {
            *length = size;
        }
    }
    return mem_ptr;
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
void *mem_alloc_and_align(size_t size, size_t alignment) {
    uint8_t align_diff = (uintptr_t) mem_alloc(0) % alignment;

    if (align_diff > 0)  // alignment needed
    {
        if (mem_alloc(alignment - align_diff) == NULL) {
            return NULL;
        }
    }
    return mem_alloc(size);
}

#endif  // HAVE_DYN_MEM_ALLOC
