#include <string.h>
#include <stdint.h>
#include "context.h"
#include "eip712.h"
#include "mem.h"
#include "sol_typenames.h"
#include "path.h"

uint8_t  *typenames_array;
uint8_t  *structs_array;
uint8_t  *current_struct_fields_array;

/**
 *
 * @return a boolean indicating if the initialization was successful or not
 */
bool    init_eip712_context(void)
{
    // init global variables
    mem_init();

    if (init_sol_typenames() == false)
        return false;

    if (init_path() == false)
    {
        return false;
    }

    // set types pointer
    if ((structs_array = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }

    // create len(types)
    *structs_array = 0;
    return true;
}
