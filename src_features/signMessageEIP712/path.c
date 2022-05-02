#include <stdint.h>
#include <string.h>
#include "path.h"
#include "mem.h"
#include "context.h"

uint8_t  *path_indexes;

/**
 * Allocates the the path indexes in memory and sets them all to 0 with a count of 1.
 */
bool    init_path(void)
{
    // + 1 for the used index count
    if ((path_indexes = mem_alloc(sizeof(uint8_t) * (MAX_PATH_DEPTH + 1))) != NULL)
    {
        // set all to 0
        explicit_bzero(path_indexes + 1, sizeof(uint8_t) * MAX_PATH_DEPTH);
        *path_indexes = 1; // init count at 1, so the default path will be 0
    }
    return path_indexes != NULL;
}
