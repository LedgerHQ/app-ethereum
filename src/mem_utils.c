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
const char *mem_alloc_and_format_uint_impl(uint32_t value, const char *file, int line) {
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
    if ((mem_ptr = app_mem_alloc_impl(sizeof(char) * (size + 1), false, file, line))) {
        snprintf(mem_ptr, (size + 1), "%u", value);
    }
    return mem_ptr;
}

char *app_mem_strdup_impl(const char *src, const char *file, int line) {
    char *dst;
    size_t length = strlen(src) + 1;

    if ((dst = app_mem_alloc_impl(length, false, file, line)) != NULL) {
        memcpy(dst, src, length);
    }
    return dst;
}

/**
 * Allocate buffer and Clear it
 *
 * @param[out] buffer Pointer to the buffer to allocate
 * @param[in] size Size of the buffer to allocate
 * @param[in] persistent Whether the buffer should be persistent
 * @return true if the allocation was successful, false otherwise
 */
bool mem_buffer_allocate_impl(void **buffer,
                              uint16_t size,
                              bool persistent,
                              const char *file,
                              int line) {
    if (size != 0) {
        // Check if the buffer is already allocated
        if (*buffer != NULL) {
            PRINTF("Buffer already allocated, freeing it before reallocating\n");
            app_mem_free_impl(*buffer, file, line);
        }
        // Allocate the Title message buffer
        if ((*buffer = app_mem_alloc_impl(size, persistent, file, line)) == NULL) {
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
void mem_buffer_cleanup_impl(void **buffer, const char *file, int line) {
    if (*buffer != NULL) {
        app_mem_free_impl(*buffer, file, line);
        *buffer = NULL;
    }
}
