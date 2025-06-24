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

char *app_mem_strdup(const char *src) {
    char *dst;
    size_t length = strlen(src) + 1;

    if ((dst = app_mem_alloc(length)) != NULL) {
        memcpy(dst, src, length);
    }
    return dst;
}
