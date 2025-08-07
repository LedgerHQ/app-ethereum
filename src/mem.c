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

#define SIZE_MEM_BUFFER (1024 * 16)

static uint8_t mem_buffer[SIZE_MEM_BUFFER] __attribute__((aligned(sizeof(intmax_t))));
static mem_ctx_t mem_ctx = NULL;

#ifdef HAVE_MEMORY_PROFILING
#define MP_LOG_PREFIX "==MP "
#endif

bool app_mem_init(void) {
    void *buf = mem_buffer;
    size_t buf_size = sizeof(mem_buffer);

    mem_ctx = mem_init(buf, buf_size);
#ifdef HAVE_MEMORY_PROFILING
    PRINTF(MP_LOG_PREFIX "init;0x%p;%u\n", buf, buf_size);
#endif
    return mem_ctx != NULL;
}

void *app_mem_alloc_impl(size_t size, const char *file, int line) {
    void *ptr;
    ptr = mem_alloc(mem_ctx, size);
#ifdef HAVE_MEMORY_PROFILING
    PRINTF(MP_LOG_PREFIX "alloc;%u;0x%p;%s:%u\n", size, ptr, file, line);
#else
    (void) file;
    (void) line;
#endif
    return ptr;
}

void app_mem_free_impl(void *ptr, const char *file, int line) {
#ifdef HAVE_MEMORY_PROFILING
    PRINTF(MP_LOG_PREFIX "free;0x%p;%s:%u\n", ptr, file, line);
#else
    (void) file;
    (void) line;
#endif
    mem_free(mem_ctx, ptr);
}
