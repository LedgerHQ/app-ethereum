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
#include "mem_alloc.h"
#include "os_print.h"

#define SIZE_MEM_BUFFER 10240

static uint8_t mem_buffer[SIZE_MEM_BUFFER] __attribute__((aligned(8)));
static uint16_t mem_legacy_idx;
static mem_ctx_t mem_ctx = NULL;

bool app_mem_init(void) {
    mem_ctx = mem_init(mem_buffer, sizeof(mem_buffer));
    return mem_ctx != NULL;
}

void *app_mem_alloc(size_t size) {
    return mem_alloc(mem_ctx, size);
}

void app_mem_free(void *ptr) {
    mem_free(mem_ctx, ptr);
}

/**
 * Initializes the memory buffer index
 */
void mem_legacy_init(void) {
    mem_legacy_idx = 0;
}

/**
 * Resets the memory buffer index
 */
void mem_legacy_reset(void) {
    mem_legacy_init();
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
void *mem_legacy_alloc(size_t size) {
    size_t new_idx;

    if (__builtin_add_overflow((size_t) mem_legacy_idx, size, &new_idx)) {
        PRINTF("Error: overflow detected!\n");
        return NULL;
    }
    // Buffer exceeded
    if (new_idx > sizeof(mem_buffer)) {
        PRINTF("Error: mem_alloc(%u) failed!\n", size);
        return NULL;
    }
    mem_legacy_idx += size;
    return &mem_buffer[mem_legacy_idx - size];
}

/**
 * De-allocates (pop) a chunk of memory buffer by a given size.
 *
 * @param[in] size Requested deallocation size in bytes
 */
void mem_legacy_dealloc(size_t size) {
    // More than is already allocated
    if (size > mem_legacy_idx) {
        PRINTF("Warning: mem_dealloc(%u) with a value larger than allocated!\n", size);
        mem_legacy_idx = 0;
    } else {
        mem_legacy_idx -= size;
    }
}

#endif  // HAVE_DYN_MEM_ALLOC
