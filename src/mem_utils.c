#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "os_print.h"
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

/**
 * Allocate buffer and Clear it
 *
 * @param[out] buffer Pointer to the buffer to allocate
 * @param[in] size Size of the buffer to allocate
 * @return true if the allocation was successful, false otherwise
 */
bool mem_buffer_allocate(void **buffer, uint16_t size) {
    if (size != 0) {
        // Check if the buffer is already allocated
        if (*buffer != NULL) {
            PRINTF("Buffer already allocated, freeing it before reallocating\n");
            app_mem_free(*buffer);
        }
        // Allocate the Title message buffer
        if ((*buffer = app_mem_alloc(size)) == NULL) {
            PRINTF("Memory allocation failed for buffer of size %u\n", size);
            return false;
        }
        explicit_bzero(*buffer, size);
    }
    return true;
}

/**
 * Cleanup allocated memory
 *
 * @param[in] buffer Pointer to the buffer to deallocate
 */
void mem_buffer_cleanup(void **buffer) {
    if (*buffer != NULL) {
        app_mem_free(*buffer);
        *buffer = NULL;
    }
}
