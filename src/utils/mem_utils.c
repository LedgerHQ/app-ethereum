#include <stdio.h>
#include <stdint.h>
#include "app_mem_utils.h"
#include "mem_utils.h"

#define SIZE_MEM_BUFFER (1024 * 16)

static uint8_t mem_buffer[SIZE_MEM_BUFFER] __attribute__((aligned(sizeof(intmax_t))));

/**
 * Initialize the memory buffer.
 *
 * @return true if the initialization succeeded, false otherwise
 */
bool app_mem_init(void) {
    return mem_utils_init(mem_buffer, sizeof(mem_buffer));
}

/**
 * Format an unsigned 32-bit value as a string and allocate memory for it.
 *
 * @param[in] value Value to format
 * @return Pointer to allocated string or NULL if allocation failed
 */
const char *mem_alloc_and_format_uint(uint32_t value) {
    char *mem_ptr;
    uint32_t value_copy;
    uint8_t size;

    // Calculate required size
    size = 1;  // minimum size, even if 0
    value_copy = value;
    while (value_copy >= 10) {
        value_copy /= 10;
        size += 1;
    }

    // +1 for null terminator
    if ((mem_ptr = APP_MEM_ALLOC(sizeof(char) * (size + 1))) != NULL) {
        snprintf(mem_ptr, (size + 1), "%u", value);
    }
    return mem_ptr;
}
