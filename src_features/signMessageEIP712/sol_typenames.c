#include <stdint.h>
#include <string.h>
#include "sol_typenames.h"
#include "eip712.h"
#include "context.h"
#include "mem.h"

// Bit indicating they are more types associated to this typename
#define TYPENAME_MORE_TYPE  (1 << 7)

enum
{
    IDX_ENUM = 0,
    IDX_STR_IDX,
    IDX_COUNT
};

bool init_sol_typenames(void)
{
    const char *const typenames_str[] = {
        "int",      // 0
        "uint",     // 1
        "address",  // 2
        "bool",     // 3
        "string",   // 4
        "byte",     // 5
        "bytes"     // 6
    };
    const uint8_t enum_to_idx[][IDX_COUNT] = {
        { TYPE_SOL_INT,       0 },
        { TYPE_SOL_UINT,      1 },
        { TYPE_SOL_ADDRESS,   2 },
        { TYPE_SOL_BOOL,      3 },
        { TYPE_SOL_STRING,    4 },
        { TYPE_SOL_BYTE,      5 },
        { TYPE_SOL_BYTES_FIX, 6 },
        { TYPE_SOL_BYTES_DYN, 6 }
    };
    uint8_t *previous_match;
    uint8_t *typename_len_ptr;
    char *typename_ptr;

    if ((typenames_array = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *typenames_array = 0;
    // loop over typenames
    for (size_t s_idx = 0;
         s_idx < (sizeof(typenames_str) / sizeof(typenames_str[IDX_ENUM]));
         ++s_idx)
    {
        previous_match = NULL;
        // loop over enum/typename pairs
        for (size_t e_idx = 0;
             e_idx < (sizeof(enum_to_idx) / sizeof(enum_to_idx[IDX_ENUM]));
             ++e_idx)
        {
            if (s_idx == (size_t)enum_to_idx[e_idx][IDX_STR_IDX]) // match
            {
                if (previous_match) // in case of a previous match, mark it
                {
                    *previous_match |= TYPENAME_MORE_TYPE;
                }
                if ((previous_match = mem_alloc(sizeof(uint8_t))) == NULL)
                {
                    return false;
                }
                *previous_match = enum_to_idx[e_idx][IDX_ENUM];
            }
        }

        if (previous_match) // if at least one match was found
        {
            if ((typename_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
            {
                return false;
            }
            // get pointer to the allocated space just above
            *typename_len_ptr = strlen(typenames_str[s_idx]);


            if ((typename_ptr = mem_alloc(sizeof(char) * *typename_len_ptr)) == NULL)
            {
                return false;
            }
            // copy typename
            memcpy(typename_ptr, typenames_str[s_idx], *typename_len_ptr);
        }
        // increment array size
        *typenames_array += 1;
    }
    return true;
}

/**
 *
 * @param[in] field_ptr pointer to a struct field
 * @param[out] length length of the returned typename
 * @return typename or \ref NULL in case it wasn't found
 */
const char *get_struct_field_sol_typename(const uint8_t *field_ptr,
                                          uint8_t *const length)
{
    e_type field_type;
    const uint8_t *typename_ptr;
    uint8_t typenames_count;
    bool more_type;
    bool typename_found;

    field_type = struct_field_type(field_ptr);
    typename_ptr = get_array_in_mem(typenames_array, &typenames_count);
    typename_found = false;
    while (typenames_count-- > 0)
    {
        more_type = true;
        while (more_type)
        {
            more_type = *typename_ptr & TYPENAME_MORE_TYPE;
            e_type type_enum = *typename_ptr & TYPENAME_ENUM;
            if (type_enum == field_type)
            {
                typename_found = true;
            }
            typename_ptr += 1;
        }
        typename_ptr = (uint8_t*)get_string_in_mem(typename_ptr, length);
        if (typename_found) return (char*)typename_ptr;
        typename_ptr += *length;
    }
    return NULL; // Not found
}
