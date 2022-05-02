#include <stdlib.h>
#include <stdio.h>
#include "eip712.h"
#include "mem.h"
#include "encode_type.h"
#include "type_hash.h"

const uint8_t *type_hash(const void *const structs_array,
                         const char *const struct_name,
                         const uint8_t struct_name_length)
{
    const void *const mem_loc_bak = mem_alloc(0); // backup the memory location
    const char  *typestr;
    uint16_t    length;

    typestr = encode_type(structs_array, struct_name, struct_name_length, &length);
    if (typestr == NULL)
    {
        return NULL;
    }

#ifdef DEBUG
    fwrite(typestr, sizeof(char), length, stdout);
    printf("\n");
#endif

    // restore the memory location
    mem_dealloc(mem_alloc(0) - mem_loc_bak);

    return NULL;
}
