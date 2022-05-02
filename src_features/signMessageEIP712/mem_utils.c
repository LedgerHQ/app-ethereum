#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mem.h"
#include "mem_utils.h"

void    *alloc_and_copy(const void *data, size_t size)
{
    void    *mem_ptr;

    if ((mem_ptr = mem_alloc(size)) != NULL)
    {
        memmove(mem_ptr, data, size);
    }
    return mem_ptr;
}

char    *alloc_and_copy_char(char c)
{
    return alloc_and_copy(&c, sizeof(char));
}

/**
 * Format an unsigned number up to 32-bit into memory into an ASCII string.
 *
 * @param[in] value Value to write in memory
 * @param[in] max_chars Maximum number of characters that could be written
 *
 * @return how many characters have been written in memory, 0 in case of an allocation error
 */
uint8_t format_uint_into_mem(uint32_t value, const uint8_t max_chars)
{
    char        *ptr;
    uint8_t     written_chars;

    if ((ptr = mem_alloc(sizeof(char) * max_chars)) == NULL)
    {
        return 0;
    }
    written_chars = sprintf(ptr, "%u", value);
    mem_dealloc(max_chars - written_chars); // in case it ended up being less
    return written_chars;
}
