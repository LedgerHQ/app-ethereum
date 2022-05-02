#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "eip712.h"
#include "mem.h"
#include "type_hash.h"


static uint8_t  *typenames_array;
static uint8_t  *structs_array;
static uint8_t  *current_struct_fields_array;

// lib functions
const void *get_array_in_mem(const void *ptr, uint8_t *const array_size)
{
    *array_size = *(uint8_t*)ptr;
    return (ptr + 1);
}

static inline const char *get_string_in_mem(const uint8_t *ptr, uint8_t *const string_length)
{
    return (char*)get_array_in_mem(ptr, string_length);
}

// ptr must point to the beginning of a struct field
static inline uint8_t get_struct_field_typedesc(const uint8_t *ptr)
{
    return *ptr;
}

// ptr must point to the beginning of a struct field
bool    struct_field_is_array(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & ARRAY_MASK);
}

// ptr must point to the beginning of a struct field
bool    struct_field_has_typesize(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & TYPESIZE_MASK);
}

// ptr must point to the beginning of a struct field
e_type  struct_field_type(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & TYPE_MASK);
}

// ptr must point to the beginning of a struct field
// TODO: Extra check inside or not
uint8_t get_struct_field_typesize(const uint8_t *ptr)
{
    return *(ptr + 1);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_custom_typename(const uint8_t *ptr,
                                                uint8_t *const length)
{
    ptr += 1; // skip TypeDesc
    return get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_sol_typename(const uint8_t *ptr,
                                             uint8_t *const length)
{
    e_type field_type;
    const uint8_t *typename_ptr;
    uint8_t typenames_count;
    bool more_type;
    bool typename_found;

    field_type = struct_field_type(ptr);
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

// ptr must point to the beginning of a struct field
const char *get_struct_field_typename(const uint8_t *ptr,
                                      uint8_t *const length)
{
    if (struct_field_type(ptr) == TYPE_CUSTOM)
    {
        return get_struct_field_custom_typename(ptr, length);
    }
    return get_struct_field_sol_typename(ptr, length);
}

// ptr must point to the beginning of a depth level
e_array_type struct_field_array_depth(const uint8_t *ptr,
                                      uint8_t *const array_size)
{
    if (*ptr == ARRAY_FIXED_SIZE)
    {
        *array_size = *(ptr + 1);
    }
    return *ptr;
}

// ptr must point to the beginning of a struct field level
const uint8_t *get_next_struct_field_array_lvl(const uint8_t *ptr)
{
    switch (*ptr)
    {
        case ARRAY_DYNAMIC:
            break;
        case ARRAY_FIXED_SIZE:
            ptr += 1;
            break;
        default:
            // should not be in here :^)
            break;
    }
    return ptr + 1;
}

// Skips TypeDesc and TypeSize/Length+TypeName
// Came to be since it is used in multiple functions
// TODO: Find better name
const uint8_t *struct_field_half_skip(const uint8_t *ptr)
{
    const uint8_t *field_ptr;
    uint8_t size;

    field_ptr = ptr;
    ptr += 1; // skip TypeDesc
    if (struct_field_type(field_ptr) == TYPE_CUSTOM)
    {
        get_string_in_mem(ptr, &size);
        ptr += (1 + size); // skip typename
    }
    else if (struct_field_has_typesize(field_ptr))
    {
        ptr += 1; // skip TypeSize
    }
    return ptr;
}

// ptr must point to the beginning of a struct field
const uint8_t *get_struct_field_array_lvls_array(const uint8_t *ptr,
                                                 uint8_t *const length)
{
    ptr = struct_field_half_skip(ptr);
    return get_array_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_keyname(const uint8_t *ptr,
                                     uint8_t *const length)
{
    const uint8_t *field_ptr;
    uint8_t size;

    field_ptr = ptr;
    ptr = struct_field_half_skip(ptr);
    if (struct_field_is_array(field_ptr))
    {
        ptr = get_array_in_mem(ptr, &size);
        while (size-- > 0)
        {
            ptr = get_next_struct_field_array_lvl(ptr);
        }
    }
    return get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const uint8_t *get_next_struct_field(const void *ptr)
{
    uint8_t length;

    ptr = (uint8_t*)get_struct_field_keyname(ptr, &length);
    return (ptr + length);
}

// ptr must point to the beginning of a struct
const char *get_struct_name(const uint8_t *ptr, uint8_t *const length)
{
    return (char*)get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct
const uint8_t *get_struct_fields_array(const uint8_t *ptr,
                                       uint8_t *const length)
{
    uint8_t name_length;

    get_struct_name(ptr, &name_length);
    ptr += (1 + name_length); // skip length
    return get_array_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct
const uint8_t *get_next_struct(const uint8_t *ptr)
{
    uint8_t fields_count;

    ptr = get_struct_fields_array(ptr, &fields_count);
    while (fields_count-- > 0)
    {
        ptr = get_next_struct_field(ptr);
    }
    return ptr;
}

// ptr must point to the size of the structs array
const uint8_t *get_structs_array(const uint8_t *ptr, uint8_t *const length)
{
    return get_array_in_mem(ptr, length);
}

// Finds struct with a given name
const uint8_t *get_structn(const uint8_t *const ptr,
                           const char *const name_ptr,
                           const uint8_t name_length)
{
    uint8_t structs_count;
    const uint8_t *struct_ptr;
    const char *name;
    uint8_t length;

    struct_ptr = get_structs_array(ptr, &structs_count);
    while (structs_count-- > 0)
    {
        name = get_struct_name(struct_ptr, &length);
        if ((name_length == length) && (memcmp(name, name_ptr, length) == 0))
        {
            return struct_ptr;
        }
        struct_ptr = get_next_struct(struct_ptr);
    }
    return NULL;
}

static inline const uint8_t *get_struct(const uint8_t *const ptr,
                                        const char *const name_ptr)
{
    return get_structn(ptr, name_ptr, strlen(name_ptr));
}
//

bool    set_struct_name(const uint8_t *const data)
{
    uint8_t *length_ptr;
    char *name_ptr;

    // increment number of structs
    *structs_array += 1;

    // copy length
    if ((length_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *length_ptr = data[OFFSET_LC];

    // copy name
    if ((name_ptr = mem_alloc(sizeof(char) * data[OFFSET_LC])) == NULL)
    {
        return false;
    }
    memmove(name_ptr, &data[OFFSET_DATA], data[OFFSET_LC]);

    // initialize number of fields
    if ((current_struct_fields_array = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *current_struct_fields_array = 0;
    return true;
}

bool    set_struct_field(const uint8_t *const data)
{
    uint8_t data_idx = OFFSET_DATA;
    uint8_t *type_desc_ptr;
    uint8_t *type_size_ptr;
    uint8_t *typename_len_ptr;
    char *typename;
    uint8_t *array_levels_count;
    e_array_type *array_level;
    uint8_t *array_level_size;
    uint8_t *fieldname_len_ptr;
    char *fieldname_ptr;

    // increment number of struct fields
    *current_struct_fields_array += 1;

    // copy TypeDesc
    if ((type_desc_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *type_desc_ptr = data[data_idx++];

    // check TypeSize flag in TypeDesc
    if (*type_desc_ptr & TYPESIZE_MASK)
    {
        // copy TypeSize
        if ((type_size_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            return false;
        }
        *type_size_ptr = data[data_idx++];
    }
    else if ((*type_desc_ptr & TYPE_MASK) == TYPE_CUSTOM)
    {
        // copy custom struct name length
        if ((typename_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            return false;
        }
        *typename_len_ptr = data[data_idx++];

        // copy name
        if ((typename = mem_alloc(sizeof(char) * *typename_len_ptr)) == NULL)
        {
            return false;
        }
        memmove(typename, &data[data_idx], *typename_len_ptr);
        data_idx += *typename_len_ptr;
    }
    if (*type_desc_ptr & ARRAY_MASK)
    {
        if ((array_levels_count = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            return false;
        }
        *array_levels_count = data[data_idx++];
        for (int idx = 0; idx < *array_levels_count; ++idx)
        {
            if ((array_level = mem_alloc(sizeof(uint8_t))) == NULL)
            {
                return false;
            }
            *array_level = data[data_idx++];
            switch (*array_level)
            {
                case ARRAY_DYNAMIC: // nothing to do
                    break;
                case ARRAY_FIXED_SIZE:
                    if ((array_level_size = mem_alloc(sizeof(uint8_t))) == NULL)
                    {
                        return false;
                    }
                    *array_level_size = data[data_idx++];
                    break;
                default:
                    // should not be in here :^)
                    break;
            }
        }
    }

    // copy length
    if ((fieldname_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *fieldname_len_ptr = data[data_idx++];

    // copy name
    if ((fieldname_ptr = mem_alloc(sizeof(char) * *fieldname_len_ptr)) == NULL)
    {
        return false;
    }
    memmove(fieldname_ptr, &data[data_idx], *fieldname_len_ptr);
    return true;
}


bool    handle_apdu(const uint8_t *const data)
{
    switch (data[OFFSET_INS])
    {
        case INS_STRUCT_DEF:
            switch (data[OFFSET_P2])
            {
                case P2_NAME:
                    set_struct_name(data);
                    break;
                case P2_FIELD:
                    set_struct_field(data);
                    break;
                default:
                    printf("Unknown P2 0x%x for APDU 0x%x\n", data[OFFSET_P2], data[OFFSET_INS]);
                    return false;
            }
            break;
        case INS_STRUCT_IMPL:
            switch (data[OFFSET_P2])
            {
                case P2_NAME:
                    type_hash(structs_array, (char*)&data[OFFSET_DATA], data[OFFSET_LC]);
                    break;
                case P2_FIELD:
                    break;
                case P2_ARRAY:
                    break;
                default:
                    printf("Unknown P2 0x%x for APDU 0x%x\n", data[OFFSET_P2], data[OFFSET_INS]);
                    return false;
            }
            break;
        default:
            printf("Unrecognized APDU (0x%.02x)\n", data[OFFSET_INS]);
            return false;
    }
    return true;
}

bool    init_typenames(void)
{
    const char *const typenames_str[] = {
        "int",
        "uint",
        "address",
        "bool",
        "string",
        "byte",
        "bytes"
    };
    const int enum_to_idx[][IDX_COUNT] = {
        { TYPE_SOL_INT, 0 },
        { TYPE_SOL_UINT, 1},
        { TYPE_SOL_ADDRESS, 2 },
        { TYPE_SOL_BOOL, 3 },
        { TYPE_SOL_STRING, 4 },
        { TYPE_SOL_BYTE, 5 },
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

bool    init_eip712_context(void)
{
    // init global variables
    init_mem();

    if (init_typenames() == false)
        return false;

    // set types pointer
    if ((structs_array = mem_alloc(sizeof(uint8_t))) == NULL)
        return false;

    // create len(types)
    *structs_array = 0;
    return true;
}

int     main(void)
{
    uint8_t         buf[260]; // 4 bytes APDU header + 256 bytes payload
    uint16_t        idx;
    int             state;
    uint8_t         payload_size = 0;

    init_eip712_context();

    state = OFFSET_CLA;
    idx = 0;
    while (fread(&buf[idx], sizeof(buf[idx]), 1, stdin) > 0)
    {
        switch (state)
        {
            case OFFSET_CLA:
            case OFFSET_INS:
            case OFFSET_P1:
            case OFFSET_P2:
                state += 1;
                idx += 1;
                break;
            case OFFSET_LC:
                payload_size = buf[idx];
                state = OFFSET_DATA;
                idx += 1;
                break;
            case OFFSET_DATA:
                if (--payload_size == 0)
                {
                    if (!handle_apdu(buf)) return false;
                    state = OFFSET_CLA;
                    idx = 0;
                }
                else idx += 1;
                break;
            default:
                printf("Unexpected APDU state!\n");
                return EXIT_FAILURE;
        }
    }
#ifdef DEBUG
    printf("\n%lu bytes used in RAM\n", (mem_max + 1));
#endif
    return EXIT_SUCCESS;
}
