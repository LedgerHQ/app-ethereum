#include <stdint.h>
#ifdef DEBUG
#include <stdio.h>
#endif
#include "mem.h"


#define SIZE_MEM_BUFFER 1024

static uint8_t  mem_buffer[SIZE_MEM_BUFFER];
static size_t   mem_idx;
#ifdef DEBUG
size_t  mem_max;
#endif


/**
 * Initializes the memory buffer index
 */
void    mem_init(void)
{
    mem_idx = 0;
#ifdef DEBUG
    mem_max = 0;
#endif
}

/**
 * Resets the memory buffer index
 */
void    mem_reset(void)
{
    mem_init();
}

/**
 * Allocates a chunk of the memory buffer of a given size.
 * Checks to see if there are enough space left in the memory buffer, returns
 * the current location in the memory buffer and moves the index accordingly.
 *
 * @param[in] size Requested allocation size in bytes
 * @return Allocated memory pointer; \ref NULL if not enough space left.
 */
void    *mem_alloc(size_t size)
{
    if ((mem_idx + size) > SIZE_MEM_BUFFER) // Buffer exceeded
    {
#ifdef DEBUG
        printf("Memory exhausted!\n");
#endif
        return NULL;
    }
    mem_idx += size;
#ifdef DEBUG
    if (mem_idx > mem_max)
    {
        mem_max = mem_idx;
    }
#endif
    return &mem_buffer[mem_idx - size];
}


/**
 * De-allocates a chunk of memory buffer by a given size.
 *
 * @param[in] size Requested deallocation size in bytes
 */
void    mem_dealloc(size_t size)
{
    if (size > mem_idx) // More than is already allocated
    {
        mem_idx = 0;
    }
    else
    {
        mem_idx -= size;
    }
}
