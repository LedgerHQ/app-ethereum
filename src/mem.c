/**
 * Dynamic allocator that uses a fixed-length buffer that is hopefully big enough
 *
 * The two functions alloc & dealloc use the buffer as a simple stack.
 * Especially useful when an unpredictable amount of data will be received and have to be stored
 * during the transaction but discarded right after.
 */

#include <stdint.h>
#include "mem.h"
#include "mem_alloc.h"
#include "os_print.h"

#define SIZE_MEM_BUFFER_MAIN 10240
#define SIZE_MEM_BUFFER_ALT  2048
#define SIZE_MEM_BUFFER      (SIZE_MEM_BUFFER_MAIN + SIZE_MEM_BUFFER_ALT)

static uint8_t mem_buffer[SIZE_MEM_BUFFER] __attribute__((aligned(sizeof(intmax_t))));
static uint16_t mem_legacy_idx;
static mem_ctx_t mem_ctx = NULL;

bool app_mem_init(void) {
    mem_ctx = mem_init(mem_buffer, sizeof(mem_buffer));
    return mem_ctx != NULL;
}

void *app_mem_alloc_impl(size_t size, const char *file, int line) {
    void *ptr;
    ptr = mem_alloc(mem_ctx, size);
#ifdef HAVE_MEMORY_PROFILING
    PRINTF("==MP alloc;%u;0x%p;%s:%u\n", size, ptr, file, line);
#else
    (void) file;
    (void) line;
#endif
    return ptr;
}

void app_mem_free_impl(void *ptr, const char *file, int line) {
#ifdef HAVE_MEMORY_PROFILING
    PRINTF("==MP free;0x%p;%s:%u\n", ptr, file, line);
#else
    (void) file;
    (void) line;
#endif
    mem_free(mem_ctx, ptr);
}

/**
 * Initializes the memory buffer index
 */
void mem_legacy_init(void) {
    mem_legacy_idx = 0;
    // initialize the new allocator to still be able to use it, just in case
    mem_ctx = mem_init(mem_buffer + SIZE_MEM_BUFFER_MAIN, SIZE_MEM_BUFFER_ALT);
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
    if (new_idx > SIZE_MEM_BUFFER_MAIN) {
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
