#include <string.h>
#include <stdint.h>
#include "context.h"
#include "eip712.h"
#include "mem.h"
#include "sol_typenames.h"
#include "path.h"
#include "field_hash.h"
#include "ui_logic.h"

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
    {
        return false;
    }

    if (path_init() == false)
    {
        return false;
    }

    if (field_hash_init() == false)
    {
        return false;
    }

    if (ui_712_init() == false)
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

// TODO: Make a deinit function
