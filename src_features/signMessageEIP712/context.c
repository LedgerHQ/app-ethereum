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
bool eip712_context_initialized = false;

/**
 *
 * @return a boolean indicating if the initialization was successful or not
 */
bool    eip712_context_init(void)
{
    // init global variables
    mem_init();

    if (sol_typenames_init() == false)
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

    eip712_context_initialized = true;

    return true;
}

void    eip712_context_deinit(void)
{
    path_deinit();
    field_hash_deinit();
    ui_712_deinit();
    mem_reset();
    eip712_context_initialized = false;
}
