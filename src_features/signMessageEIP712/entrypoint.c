#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


enum {
    OFFSET_CLA = 0,
    OFFSET_INS,
    OFFSET_P1,
    OFFSET_P2,
    OFFSET_LC,
    OFFSET_DATA
};

typedef enum
{
    // contract defined struct
    TYPE_CUSTOM = 0,
    // native types
    TYPE_SOL_INT,
    TYPE_SOL_UINT,
    TYPE_SOL_ADDRESS,
    TYPE_SOL_BOOL,
    TYPE_SOL_STRING,
    TYPE_SOL_BYTE,
    TYPE_SOL_BYTES_FIX,
    TYPE_SOL_BYTES_DYN,
    TYPES_COUNT
}   e_type;

typedef enum
{
    ARRAY_DYNAMIC = 0,
    ARRAY_FIXED_SIZE
}   e_array_type;


// APDUs INS
#define INS_STRUCT_DEF  0x18
#define INS_STRUCT_IMPL 0x1A

// TypeDesc masks
#define TYPE_MASK       (0xF)
#define ARRAY_MASK      (1 << 7)
#define TYPESIZE_MASK   (1 << 6)

// Solidity typename mask
#define TYPENAME_MORE_TYPE  (1 << 7)
#define TYPENAME_ENUM       (0xF)

#define     SIZE_MEM_BUFFER 1024
uint8_t     mem_buffer[SIZE_MEM_BUFFER];
uint16_t    mem_idx;

uint8_t     *typenames_array;
uint8_t     *structs_array;
uint8_t     *current_struct_fields_array;

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
uint8_t get_struct_field_typedesc(const uint8_t *ptr)
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
//





bool    set_struct_name(uint8_t *data)
{
    // increment number of structs
    *structs_array += 1;

    // copy length
    mem_buffer[mem_idx++] = data[OFFSET_LC];

    // copy name
    memmove(&mem_buffer[mem_idx], &data[OFFSET_DATA], data[OFFSET_LC]);
    mem_idx += data[OFFSET_LC];

    // initialize number of fields
    current_struct_fields_array = &mem_buffer[mem_idx];
    mem_buffer[mem_idx++] = 0;
    return true;
}

bool    set_struct_field(uint8_t *data)
{
    uint8_t data_idx = OFFSET_DATA;
    uint8_t type_desc, len;

    // increment number of struct fields
    *current_struct_fields_array += 1;

    // copy TypeDesc
    type_desc = data[data_idx++];
    mem_buffer[mem_idx++] = type_desc;

    // check TypeSize flag in TypeDesc
    if (type_desc & TYPESIZE_MASK)
    {
        // copy TypeSize
        mem_buffer[mem_idx++] = data[data_idx++];
    }
    else if ((type_desc & TYPE_MASK) == TYPE_CUSTOM)
    {
        len = data[data_idx++];
        // copy custom struct name length
        mem_buffer[mem_idx++] = len;
        // copy name
        memmove(&mem_buffer[mem_idx], &data[data_idx], len);
        mem_idx += len;
        data_idx += len;
    }
    if (type_desc & ARRAY_MASK)
    {
        len = data[data_idx++];
        mem_buffer[mem_idx++] = len;
        while (len-- > 0)
        {
            mem_buffer[mem_idx++] = data[data_idx];
            switch (data[data_idx++])
            {
                case ARRAY_DYNAMIC: // nothing to do
                    break;
                case ARRAY_FIXED_SIZE:
                    mem_buffer[mem_idx++] = data[data_idx++];
                    break;
                default:
                    // should not be in here :^)
                    break;
            }
        }
    }

    // copy length
    len = data[data_idx++];
    mem_buffer[mem_idx++] = len;

    // copy name
    memmove(&mem_buffer[mem_idx], &data[data_idx], len);
    mem_idx += len;
    return true;
}


void    dump_mem(void)
{
    uint8_t structs_count;
    const uint8_t *struct_ptr;
    //
    uint8_t fields_count;
    const uint8_t *field_ptr;
    //
    uint8_t lvls_count;
    const uint8_t *lvl_ptr;

    const uint8_t *name;
    uint8_t name_length;
    //
    uint8_t array_size;
    uint8_t byte_size;


    struct_ptr = get_structs_array(structs_array, &structs_count);
    while (structs_count-- > 0)
    {
        name = get_struct_name(struct_ptr, &name_length);
        fwrite(name, sizeof(*name), name_length, stdout);
        printf("(");
        field_ptr = get_struct_fields_array(struct_ptr, &fields_count);
        for (int idx = 0; idx < fields_count; ++idx)
        {
            if (idx > 0) printf(",");
            name = get_struct_field_typename(field_ptr, &name_length);
            fwrite(name, sizeof(*name), name_length, stdout);
            if (struct_field_has_typesize(field_ptr))
            {
                byte_size = get_struct_field_typesize(field_ptr);
                switch(struct_field_type(field_ptr))
                {
                    case TYPE_SOL_INT:
                    case TYPE_SOL_UINT:
                        printf("%u", (byte_size * 8));
                        break;
                    case TYPE_SOL_BYTES_FIX:
                        printf("%u", byte_size);
                        break;
                    default:
                        // should not be in here :^)
                        break;
                }
            }
            if (struct_field_is_array(field_ptr))
            {
                lvl_ptr = get_struct_field_array_lvls_array(field_ptr, &lvls_count);
                while (lvls_count-- > 0)
                {
                    printf("[");
                    switch (struct_field_array_depth(lvl_ptr, &array_size))
                    {
                        case ARRAY_DYNAMIC:
                            break;
                        case ARRAY_FIXED_SIZE:
                            printf("%u", array_size);
                            break;
                        default:
                            // should not be in here :^)
                            break;
                    }
                    printf("]");
                    lvl_ptr = get_next_struct_field_array_lvl(lvl_ptr);
                }
            }
            printf(" ");
            name = get_struct_field_keyname(field_ptr, &name_length);
            fwrite(name, sizeof(*name), name_length, stdout);

            field_ptr = get_next_struct_field(field_ptr);
        }
        printf(")\n");
        struct_ptr = get_next_struct(struct_ptr);
    }
    return;
}

bool    handle_apdu(uint8_t *data)
{
    switch (data[OFFSET_INS])
    {
        case INS_STRUCT_DEF:
            switch (data[OFFSET_P2])
            {
                case 0x00:
                    set_struct_name(data);
                    break;
                case 0xFF:
                    set_struct_field(data);
                    break;
                default:
                    printf("Unknown P2 0x%x for APDU 0x%x\n", data[OFFSET_P2], data[OFFSET_INS]);
                    return false;
            }
            break;
        case INS_STRUCT_IMPL:
            break;
        default:
            printf("Unrecognized APDU");
            return false;
    }
    return true;
}

bool    init_typenames(void)
{
    char    *typenames_str[] = {
        "int",
        "uint",
        "address",
        "bool",
        "string",
        "byte",
        "bytes"
    };
    int     enum_to_idx[][2] = {
        { TYPE_SOL_INT, 0 },
        { TYPE_SOL_UINT, 1},
        { TYPE_SOL_ADDRESS, 2 },
        { TYPE_SOL_BOOL, 3 },
        { TYPE_SOL_STRING, 4 },
        { TYPE_SOL_BYTE, 5 },
        { TYPE_SOL_BYTES_FIX, 6 },
        { TYPE_SOL_BYTES_DYN, 6 }
    };
    bool    first_match;

    typenames_array = &mem_buffer[mem_idx++];
    *typenames_array = 0;
    // loop over typenames
    for (size_t s_idx = 0;
         s_idx < (sizeof(typenames_str) / sizeof(typenames_str[0]));
         ++s_idx)
    {
        first_match = true;
        // loop over enum/typename pairs
        for (size_t e_idx = 0;
             e_idx < (sizeof(enum_to_idx) / sizeof(enum_to_idx[0]));
             ++e_idx)
        {
            if (s_idx == (size_t)enum_to_idx[e_idx][1]) // match
            {
                mem_buffer[mem_idx] = enum_to_idx[e_idx][0];
                if (!first_match) // in case of a previous match, mark it
                {
                    mem_buffer[mem_idx - 1] |= TYPENAME_MORE_TYPE;
                }
                mem_idx += 1;
                first_match = false;
            }
        }

        if (!first_match) // if at least one match was found
        {
            mem_buffer[mem_idx++] = strlen(typenames_str[s_idx]);
            // copy typename
            memcpy(&mem_buffer[mem_idx],
                   typenames_str[s_idx],
                   mem_buffer[mem_idx - 1]);
            // increment mem idx by typename length
            mem_idx += mem_buffer[mem_idx - 1];
        }
        // increment array size
        *typenames_array += 1;
    }
    return true;
}

void    init_heap(void)
{
    // init global variables
    mem_idx = 0;

    init_typenames();

    // set types pointer
    structs_array = &mem_buffer[mem_idx];

    // create len(types)
    mem_buffer[mem_idx++] = 0;
}

int     main(void)
{
    uint8_t         buf[256];
    uint8_t         idx;
    int             state;
    uint8_t         payload_size = 0;

    init_heap();

    state = OFFSET_CLA;
    idx = 0;
    while (true)
    {
        if (fread(&buf[idx], sizeof(buf[0]), 1, stdin) == 0) break;
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
                    handle_apdu(buf);
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
    dump_mem();
    printf("\n%d bytes used in RAM\n", mem_idx);
    return EXIT_SUCCESS;
}
