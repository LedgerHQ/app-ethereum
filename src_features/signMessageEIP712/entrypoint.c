#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "eip712.h"
#include "mem.h"


static uint8_t  *typenames_array;
static uint8_t  *structs_array;
static uint8_t  *current_struct_fields_array;

// lib functions
const uint8_t *get_array_in_mem(const uint8_t *ptr, uint8_t *const array_size)
{
    *array_size = *ptr;
    return (ptr + 1);
}

static inline const uint8_t *get_string_in_mem(const uint8_t *ptr, uint8_t *const string_length)
{
    return get_array_in_mem(ptr, string_length);
}

// ptr must point to the beginning of a struct field
static inline uint8_t get_struct_field_typedesc(const uint8_t *ptr)
{
    return *ptr;
}

// ptr must point to the beginning of a struct field
static inline bool    struct_field_is_array(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & ARRAY_MASK);
}

// ptr must point to the beginning of a struct field
static inline bool    struct_field_has_typesize(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & TYPESIZE_MASK);
}

// ptr must point to the beginning of a struct field
static inline e_type  struct_field_type(const uint8_t *ptr)
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
const uint8_t *get_struct_field_custom_typename(const uint8_t *ptr,
                                                uint8_t *const length)
{
    ptr += 1; // skip TypeDesc
    return get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const uint8_t *get_struct_field_sol_typename(const uint8_t *ptr,
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
        typename_ptr = get_string_in_mem(typename_ptr, length);
        if (typename_found) return typename_ptr;
        typename_ptr += *length;
    }
    return NULL; // Not found
}

// ptr must point to the beginning of a struct field
const uint8_t *get_struct_field_typename(const uint8_t *ptr,
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
const uint8_t *get_struct_field_keyname(const uint8_t *ptr,
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
const uint8_t *get_next_struct_field(const uint8_t *ptr)
{
    uint8_t length;

    ptr = get_struct_field_keyname(ptr, &length);
    return (ptr + length);
}

// ptr must point to the beginning of a struct
const uint8_t *get_struct_name(const uint8_t *ptr, uint8_t *const length)
{
    return get_string_in_mem(ptr, length);
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
                           const uint8_t *const name_ptr,
                           uint8_t name_length)
{
    uint8_t structs_count;
    const uint8_t *struct_ptr;
    const uint8_t *name;
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
    return get_structn(ptr, (uint8_t*)name_ptr, strlen(name_ptr));
}
//

bool    set_struct_name(const uint8_t *const data)
{
    // increment number of structs
    *structs_array += 1;

    // copy length
    *(uint8_t*)mem_alloc(1) = data[OFFSET_LC];

    // copy name
    memmove(mem_alloc(data[OFFSET_LC]), &data[OFFSET_DATA], data[OFFSET_LC]);

    // initialize number of fields
    current_struct_fields_array = mem_alloc(1);
    *current_struct_fields_array = 0;
    return true;
}

bool    set_struct_field(const uint8_t *const data)
{
    uint8_t data_idx = OFFSET_DATA;
    uint8_t type_desc, len;

    // increment number of struct fields
    *current_struct_fields_array += 1;

    // copy TypeDesc
    type_desc = data[data_idx++];
    *(uint8_t*)mem_alloc(1) = type_desc;

    // check TypeSize flag in TypeDesc
    if (type_desc & TYPESIZE_MASK)
    {
        // copy TypeSize
        *(uint8_t*)mem_alloc(1) = data[data_idx++];
    }
    else if ((type_desc & TYPE_MASK) == TYPE_CUSTOM)
    {
        len = data[data_idx++];
        // copy custom struct name length
        *(uint8_t*)mem_alloc(1) = len;
        // copy name
        memmove(mem_alloc(len), &data[data_idx], len);
        data_idx += len;
    }
    if (type_desc & ARRAY_MASK)
    {
        len = data[data_idx++];
        *(uint8_t*)mem_alloc(1) = len;
        while (len-- > 0)
        {
            *(uint8_t*)mem_alloc(1) = data[data_idx];
            switch (data[data_idx++])
            {
                case ARRAY_DYNAMIC: // nothing to do
                    break;
                case ARRAY_FIXED_SIZE:
                    *(uint8_t*)mem_alloc(1) = data[data_idx++];
                    break;
                default:
                    // should not be in here :^)
                    break;
            }
        }
    }

    // copy length
    len = data[data_idx++];
    *(uint8_t*)mem_alloc(1) = len;

    // copy name
    memmove(mem_alloc(len), &data[data_idx], len);
    return true;
}

/**
 * Format an unsigned number up to 32-bit into memory into an ASCII string.
 *
 * @param[in] value Value to write in memory
 * @param[in] max_chars Maximum number of characters that could be written
 *
 * @return how many characters have been written in memory
 */
uint8_t format_uint_into_mem(uint32_t value, uint8_t max_chars)
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

void    get_struct_type_string(const uint8_t *const ptr,
                               const uint8_t *const struct_name,
                               uint8_t struct_name_length)
{
    const uint8_t *const struct_ptr = get_structn(ptr,
                                                  struct_name,
                                                  struct_name_length);
    uint16_t *typestr_length;
    char *typestr;
    const uint8_t *field_ptr;
    uint8_t fields_count;
    const uint8_t *name;
    uint8_t length;
    uint16_t field_size;
    uint8_t lvls_count;
    const uint8_t *lvl_ptr;
    uint8_t array_size;

    // set length
    typestr_length = mem_alloc(sizeof(uint16_t));

    // add name
    typestr = memmove(mem_alloc(struct_name_length), struct_name, struct_name_length);

    *(char*)mem_alloc(sizeof(char)) = '(';

    field_ptr = get_struct_fields_array(struct_ptr, &fields_count);
    for (uint8_t idx = 0; idx < fields_count; ++idx)
    {
        if (idx > 0)
        {
            *(char*)mem_alloc(sizeof(char)) = ',';
        }

        name = get_struct_field_typename(field_ptr, &length);

        memmove(mem_alloc(sizeof(char) * length), name, length);

        if (struct_field_has_typesize(field_ptr))
        {
            field_size = get_struct_field_typesize(field_ptr);
            switch (struct_field_type(field_ptr))
            {
                case TYPE_SOL_INT:
                case TYPE_SOL_UINT:
                    field_size *= 8; // bytes -> bits
                    break;
                case TYPE_SOL_BYTES_FIX:
                    break;
                default:
                    // should not be in here :^)
                    break;
            }
            // max value = 256, 3 characters max
            format_uint_into_mem(field_size, 3);
        }

        if (struct_field_is_array(field_ptr))
        {
            lvl_ptr = get_struct_field_array_lvls_array(field_ptr, &lvls_count);
            while (lvls_count-- > 0)
            {
                *(char*)mem_alloc(sizeof(char)) = '[';
                *typestr_length += 1;
                switch (struct_field_array_depth(lvl_ptr, &array_size))
                {
                    case ARRAY_DYNAMIC:
                        break;
                    case ARRAY_FIXED_SIZE:
                        // max value = 255, 3 characters max
                        *typestr_length += format_uint_into_mem(array_size, 3);
                        break;
                    default:
                        // should not be in here :^)
                        break;
                }
                *(char*)mem_alloc(sizeof(char)) = ']';
                lvl_ptr = get_next_struct_field_array_lvl(lvl_ptr);
            }
        }
        *(char*)mem_alloc(sizeof(char)) = ' ';
        name = get_struct_field_keyname(field_ptr, &length);

        memmove(mem_alloc(sizeof(char) * length), name, length);

        field_ptr = get_next_struct_field(field_ptr);
    }
    *(char*)mem_alloc(sizeof(char)) = ')';

    // - 2 since :
    //  * typestr_length is one byte before the start of the string
    //  * mem_alloc's return will point to one byte after the end of the string
    *typestr_length = (mem_alloc(0) - (void*)typestr_length - 2);

    // FIXME: DBG
    fwrite(typestr, sizeof(char), *typestr_length, stdout);
    printf("\n");
}

void    get_type_hash(const uint8_t *const ptr,
                      const uint8_t *const struct_name,
                      uint8_t struct_name_length)
{
    void *mem_loc_bak;
    uint16_t str_length;

    // backup the memory location
    mem_loc_bak = mem_alloc(0);

    // get list of structs (own + dependencies), properly ordered

    // loop over each struct and generate string
    str_length = 0;
    get_struct_type_string(ptr, struct_name, struct_name_length);

    // restore the memory location
    mem_dealloc(mem_alloc(0) - mem_loc_bak);
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
                    get_type_hash(structs_array, &data[OFFSET_DATA], data[OFFSET_LC]);
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
    uint8_t typename_len;

    typenames_array = mem_alloc(1);
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
                previous_match = mem_alloc(1);
                *previous_match = enum_to_idx[e_idx][IDX_ENUM];
            }
        }

        if (previous_match) // if at least one match was found
        {
            typename_len = strlen(typenames_str[s_idx]);
            *(uint8_t*)mem_alloc(1) = typename_len;
            // copy typename
            memcpy(mem_alloc(typename_len),
                   typenames_str[s_idx],
                   typename_len);
        }
        // increment array size
        *typenames_array += 1;
    }
    return true;
}

void    init_heap(void)
{
    // init global variables
    init_mem();

    init_typenames();

    // set types pointer
    structs_array = mem_alloc(1);

    // create len(types)
    *structs_array = 0;
}

int     main(void)
{
    uint8_t         buf[256];
    uint16_t        idx;
    int             state;
    uint8_t         payload_size = 0;

    init_heap();

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
    //printf("\n%d bytes used in RAM\n", (mem_idx + 1));
    return EXIT_SUCCESS;
}
