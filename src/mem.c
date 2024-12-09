/**
 * Dynamic allocator that uses a fixed-length buffer that is hopefully big enough
 *
 * The two functions alloc & dealloc use the buffer as a simple stack.
 * Especially useful when an unpredictable amount of data will be received and have to be stored
 * during the transaction but discarded right after.
 */

#ifdef HAVE_DYN_MEM_ALLOC

#include <stdint.h>
#include "mem.h"
#include "os_print.h"

#define SIZE_MEM_BUFFER 10240

static uint8_t mem_buffer[SIZE_MEM_BUFFER];
static size_t mem_idx;

/**
 * Initializes the memory buffer index
 */
void mem_init(void) {
    mem_idx = 0;
}

/**
 * Resets the memory buffer index
 */
void mem_reset(void) {
    mem_init();
}

/**
 * Allocates (push) a chunk of the memory buffer of a given size.
 *
 * Checks to see if there are enough space left in the memory buffer, returns
 * the current location in the memory buffer and moves the index accordingly.
 *
 * @param[in] size Requested allocation size in bytes
 * @return Allocated memory pointer; \ref NULL if not enough space left.
 */
void *mem_alloc(size_t size) {
    // Buffer exceeded
    if ((mem_idx + size) > sizeof(mem_buffer)) {
        PRINTF("Error: mem_alloc(%u) failed!\n", size);
        return NULL;
    }
    mem_idx += size;
    return &mem_buffer[mem_idx - size];
}

/**
 * De-allocates (pop) a chunk of memory buffer by a given size.
 *
 * @param[in] size Requested deallocation size in bytes
 */
void mem_dealloc(size_t size) {
    // More than is already allocated
    if (size > mem_idx) {
        PRINTF("Warning: mem_dealloc(%u) with a value larger than allocated!\n", size);
        mem_idx = 0;
    } else {
        mem_idx -= size;
    }
}

#endif  // HAVE_DYN_MEM_ALLOC
