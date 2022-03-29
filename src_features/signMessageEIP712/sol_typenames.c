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

static bool find_enum_matches(const uint8_t (*enum_to_idx)[TYPES_COUNT - 1][IDX_COUNT], uint8_t s_idx)
{
    uint8_t *enum_match = NULL;

    // loop over enum/typename pairs
    for (uint8_t e_idx = 0; e_idx < ARRAY_SIZE(*enum_to_idx); ++e_idx)
    {
        if (s_idx == (*enum_to_idx)[e_idx][IDX_STR_IDX]) // match
        {
            if (enum_match != NULL) // in case of a previous match, mark it
            {
                *enum_match |= TYPENAME_MORE_TYPE;
            }
            if ((enum_match = mem_alloc(sizeof(uint8_t))) == NULL)
            {
                return false;
            }
            *enum_match = (*enum_to_idx)[e_idx][IDX_ENUM];
        }
    }
    return (enum_match != NULL);
}

bool    init_sol_typenames(void)
{
    const char *const typenames[] = {
        "int",      // 0
        "uint",     // 1
        "address",  // 2
        "bool",     // 3
        "string",   // 4
        "byte",     // 5
        "bytes"     // 6
    };
    // \ref TYPES_COUNT - 1 since we don't include \ref TYPE_CUSTOM
    const uint8_t enum_to_idx[TYPES_COUNT - 1][IDX_COUNT] = {
        { TYPE_SOL_INT,       0 },
        { TYPE_SOL_UINT,      1 },
        { TYPE_SOL_ADDRESS,   2 },
        { TYPE_SOL_BOOL,      3 },
        { TYPE_SOL_STRING,    4 },
        { TYPE_SOL_BYTE,      5 },
        { TYPE_SOL_BYTES_FIX, 6 },
        { TYPE_SOL_BYTES_DYN, 6 }
    };
    uint8_t *typename_len_ptr;
    char *typename_ptr;

    if ((typenames_array = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *typenames_array = 0;
    // loop over typenames
    for (uint8_t s_idx = 0; s_idx < ARRAY_SIZE(typenames); ++s_idx)
    {
        // if at least one match was found
        if (find_enum_matches(&enum_to_idx, s_idx))
        {
            if ((typename_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
            {
                return false;
            }
            // get pointer to the allocated space just above
            *typename_len_ptr = strlen(typenames[s_idx]);

            if ((typename_ptr = mem_alloc(sizeof(char) * *typename_len_ptr)) == NULL)
            {
                return false;
            }
            // copy typename
            memcpy(typename_ptr, typenames[s_idx], *typename_len_ptr);
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
