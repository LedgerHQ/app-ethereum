#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <string.h>
#include "typed_data.h"
#include "sol_typenames.h"
#include "apdu_constants.h" // APDU response codes
#include "context.h"
#include "mem.h"
#include "mem_utils.h"

static s_typed_data *typed_data = NULL;


bool    typed_data_init(void)
{
    if (typed_data == NULL)
    {
        if ((typed_data = MEM_ALLOC_AND_ALIGN_TYPE(*typed_data)) == NULL)
        {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }
        // set types pointer
        if ((typed_data->structs_array = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }

        // create len(types)
        *(typed_data->structs_array) = 0;
    }
    return true;
}

void    typed_data_deinit(void)
{
    typed_data = NULL;
}

// lib functions
const void *get_array_in_mem(const void *ptr, uint8_t *const array_size)
{
    if (ptr == NULL)
    {
        return NULL;
    }
    if (array_size)
    {
        *array_size = *(uint8_t*)ptr;
    }
    return (ptr + 1);
}

const char *get_string_in_mem(const uint8_t *ptr, uint8_t *const string_length)
{
    return (char*)get_array_in_mem(ptr, string_length);
}

// ptr must point to the beginning of a struct field
static inline uint8_t get_struct_field_typedesc(const uint8_t *ptr)
{
    if (ptr == NULL)
    {
        return 0;
    }
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
uint8_t get_struct_field_typesize(const uint8_t *ptr)
{
    if (ptr == NULL)
    {
        return 0;
    }
    return *(ptr + 1);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_custom_typename(const uint8_t *ptr,
                                             uint8_t *const length)
{
    if (ptr == NULL)
    {
        return NULL;
    }
    ptr += 1; // skip TypeDesc
    return get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_typename(const uint8_t *ptr,
                                      uint8_t *const length)
{
    if (ptr == NULL)
    {
        return NULL;
    }
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
    if (ptr == NULL)
    {
        return 0;
    }
    if (*ptr == ARRAY_FIXED_SIZE)
    {
        *array_size = *(ptr + 1);
    }
    return *ptr;
}

// ptr must point to the beginning of a struct field level
const uint8_t *get_next_struct_field_array_lvl(const uint8_t *ptr)
{
    if (ptr == NULL)
    {
        return NULL;
    }
    switch (*ptr)
    {
        case ARRAY_DYNAMIC:
            break;
        case ARRAY_FIXED_SIZE:
            ptr += 1;
            break;
        default:
            // should not be in here :^)
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return NULL;
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

    if (ptr == NULL)
    {
        return NULL;
    }
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
    if (ptr == NULL)
    {
        return NULL;
    }
    ptr = struct_field_half_skip(ptr);
    return get_array_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_keyname(const uint8_t *ptr,
                                     uint8_t *const length)
{
    const uint8_t *field_ptr;
    uint8_t size;

    if (ptr == NULL)
    {
        return NULL;
    }
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

    if (ptr == NULL)
    {
        return NULL;
    }
    ptr = (uint8_t*)get_struct_field_keyname(ptr, &length);
    return (ptr + length);
}

// ptr must point to the beginning of a struct
const char *get_struct_name(const uint8_t *ptr, uint8_t *const length)
{
    if (ptr == NULL)
    {
        return NULL;
    }
    return (char*)get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct
const uint8_t *get_struct_fields_array(const uint8_t *ptr,
                                       uint8_t *const length)
{
    uint8_t name_length;

    if (ptr == NULL)
    {
        return NULL;
    }
    get_struct_name(ptr, &name_length);
    ptr += (1 + name_length); // skip length
    return get_array_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct
const uint8_t *get_next_struct(const uint8_t *ptr)
{
    uint8_t fields_count;

    if (ptr == NULL)
    {
        return NULL;
    }
    ptr = get_struct_fields_array(ptr, &fields_count);
    while (fields_count-- > 0)
    {
        ptr = get_next_struct_field(ptr);
    }
    return ptr;
}

// ptr must point to the size of the structs array
const uint8_t *get_structs_array(uint8_t *const length)
{
    return get_array_in_mem(typed_data->structs_array, length);
}

// Finds struct with a given name
const uint8_t *get_structn(const char *const name_ptr,
                           const uint8_t name_length)
{
    uint8_t structs_count;
    const uint8_t *struct_ptr;
    const char *name;
    uint8_t length;

    if (name_ptr == NULL)
    {
        return NULL;
    }
    struct_ptr = get_structs_array(&structs_count);
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
//

bool    set_struct_name(uint8_t length, const uint8_t *const name)
{
    uint8_t *length_ptr;
    char *name_ptr;

    if ((name == NULL) || (typed_data == NULL))
    {
        return false;
    }
    // increment number of structs
    *(typed_data->structs_array) += 1;

    // copy length
    if ((length_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *length_ptr = length;

    // copy name
    if ((name_ptr = mem_alloc(sizeof(char) * length)) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    memmove(name_ptr, name, length);

    // initialize number of fields
    if ((typed_data->current_struct_fields_array = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *(typed_data->current_struct_fields_array) = 0;
    return true;
}

static bool set_struct_field_custom(const uint8_t *const data, uint8_t *data_idx)
{
    uint8_t *typename_len_ptr;
    char *typename;

    // copy custom struct name length
    if ((typename_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *typename_len_ptr = data[(*data_idx)++];

    // copy name
    if ((typename = mem_alloc(sizeof(char) * *typename_len_ptr)) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    memmove(typename, &data[*data_idx], *typename_len_ptr);
    *data_idx += *typename_len_ptr;
    return true;
}

static bool set_struct_field_array(const uint8_t *const data, uint8_t *data_idx)
{
    uint8_t *array_levels_count;
    e_array_type *array_level;
    uint8_t *array_level_size;

    if ((array_levels_count = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *array_levels_count = data[(*data_idx)++];
    for (int idx = 0; idx < *array_levels_count; ++idx)
    {
        if ((array_level = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }
        *array_level = data[(*data_idx)++];
        switch (*array_level)
        {
            case ARRAY_DYNAMIC: // nothing to do
                break;
            case ARRAY_FIXED_SIZE:
                if ((array_level_size = mem_alloc(sizeof(uint8_t))) == NULL)
                {
                    apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
                    return false;
                }
                *array_level_size = data[(*data_idx)++];
                break;
            default:
                // should not be in here :^)
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                return false;
        }
    }
    return true;
}

static bool set_struct_field_typesize(const uint8_t *const data, uint8_t *data_idx)
{
    uint8_t *type_size_ptr;

    // copy TypeSize
    if ((type_size_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *type_size_ptr = data[(*data_idx)++];
    return true;
}

bool    set_struct_field(const uint8_t *const data)
{
    uint8_t data_idx = OFFSET_CDATA;
    uint8_t *type_desc_ptr;
    uint8_t *fieldname_len_ptr;
    char *fieldname_ptr;

    if ((data == NULL) || (typed_data == NULL))
    {
        return false;
    }
    // increment number of struct fields
    *(typed_data->current_struct_fields_array) += 1;

    // copy TypeDesc
    if ((type_desc_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *type_desc_ptr = data[data_idx++];

    // check TypeSize flag in TypeDesc
    if (*type_desc_ptr & TYPESIZE_MASK)
    {
        if (set_struct_field_typesize(data, &data_idx) == false)
        {
            return false;
        }
    }
    else if ((*type_desc_ptr & TYPE_MASK) == TYPE_CUSTOM)
    {
        if (set_struct_field_custom(data, &data_idx) == false)
        {
            return false;
        }
    }
    if (*type_desc_ptr & ARRAY_MASK)
    {
        if (set_struct_field_array(data, &data_idx) == false)
        {
            return false;
        }
    }

    // copy length
    if ((fieldname_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *fieldname_len_ptr = data[data_idx++];

    // copy name
    if ((fieldname_ptr = mem_alloc(sizeof(char) * *fieldname_len_ptr)) == NULL)
    {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    memmove(fieldname_ptr, &data[data_idx], *fieldname_len_ptr);
    return true;
}

#endif // HAVE_EIP712_FULL_SUPPORT
