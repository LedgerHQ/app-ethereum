#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <string.h>
#include "typed_data.h"
#include "sol_typenames.h"
#include "apdu_constants.h"  // APDU response codes
#include "context_712.h"
#include "mem.h"
#include "mem_utils.h"

static s_typed_data *typed_data = NULL;

/**
 * Initialize the typed data context
 *
 * @return whether the memory allocation was successful
 */
bool typed_data_init(void) {
    if (typed_data == NULL) {
        if ((typed_data = MEM_ALLOC_AND_ALIGN_TYPE(*typed_data)) == NULL) {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }
        // set types pointer
        if ((typed_data->structs_array = mem_alloc(sizeof(uint8_t))) == NULL) {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }

        // create len(types)
        *(typed_data->structs_array) = 0;
    }
    return true;
}

void typed_data_deinit(void) {
    typed_data = NULL;
}

/**
 * Skip TypeDesc from a structure field
 *
 * @param[in] field_ptr pointer to the beginning of the struct field
 * @param[in] ptr pointer to the current location within the struct field
 * @return pointer to the data right after
 */
static const uint8_t *field_skip_typedesc(const uint8_t *field_ptr, const uint8_t *ptr) {
    (void) ptr;
    return field_ptr + sizeof(typedesc_t);
}

/**
 * Skip the type name from a structure field
 *
 * @param[in] field_ptr pointer to the beginning of the struct field
 * @param[in] ptr pointer to the current location within the struct field
 * @return pointer to the data right after
 */
static const uint8_t *field_skip_typename(const uint8_t *field_ptr, const uint8_t *ptr) {
    uint8_t size = 0;

    if (struct_field_type(field_ptr) == TYPE_CUSTOM) {
        get_string_in_mem(ptr, &size);
        ptr += (sizeof(size) + size);
    }
    return ptr;
}

/**
 * Skip the type size from a structure field
 *
 * @param[in] field_ptr pointer to the beginning of the struct field
 * @param[in] ptr pointer to the current location within the struct field
 * @return pointer to the data right after
 */
static const uint8_t *field_skip_typesize(const uint8_t *field_ptr, const uint8_t *ptr) {
    if (struct_field_has_typesize(field_ptr)) {
        ptr += sizeof(typesize_t);
    }
    return ptr;
}

/**
 * Skip the array levels from a structure field
 *
 * @param[in] field_ptr pointer to the beginning of the struct field
 * @param[in] ptr pointer to the current location within the struct field
 * @return pointer to the data right after
 */
static const uint8_t *field_skip_array_levels(const uint8_t *field_ptr, const uint8_t *ptr) {
    uint8_t size = 0;

    if (struct_field_is_array(field_ptr)) {
        ptr = get_array_in_mem(ptr, &size);
        while (size-- > 0) {
            ptr = get_next_struct_field_array_lvl(ptr);
        }
    }
    return ptr;
}

/**
 * Skip the key name from a structure field
 *
 * @param[in] field_ptr pointer to the beginning of the struct field
 * @param[in] ptr pointer to the current location within the struct field
 * @return pointer to the data right after
 */
static const uint8_t *field_skip_keyname(const uint8_t *field_ptr, const uint8_t *ptr) {
    uint8_t size = 0;
    uint8_t *new_ptr;

    (void) field_ptr;
    new_ptr = (uint8_t *) get_array_in_mem(ptr, &size);
    return (const uint8_t *) (new_ptr + size);
}

/**
 * Get data pointer & array size from a given pointer
 *
 * @param[in] ptr given pointer
 * @param[out] array_size pointer to array size
 * @return pointer to data
 */
const void *get_array_in_mem(const void *ptr, uint8_t *const array_size) {
    if (ptr == NULL) {
        return NULL;
    }
    if (array_size) {
        *array_size = *(uint8_t *) ptr;
    }
    return (ptr + sizeof(*array_size));
}

/**
 * Get pointer to beginning of string & its length from a given pointer
 *
 * @param[in] ptr given pointer
 * @param[out] string_length pointer to string length
 * @return pointer to beginning of the string
 */
const char *get_string_in_mem(const uint8_t *ptr, uint8_t *const string_length) {
    return (char *) get_array_in_mem(ptr, string_length);
}

/**
 * Get the TypeDesc from a given struct field pointer
 *
 * @param[in] field_ptr struct field pointer
 * @return TypeDesc
 */
static inline typedesc_t get_struct_field_typedesc(const uint8_t *const field_ptr) {
    if (field_ptr == NULL) {
        return 0;
    }
    return *field_ptr;
}

/**
 * Check whether a struct field is an array
 *
 * @param[in] field_ptr struct field pointer
 * @return bool whether it is the case
 */
bool struct_field_is_array(const uint8_t *const field_ptr) {
    return (get_struct_field_typedesc(field_ptr) & ARRAY_MASK);
}

/**
 * Check whether a struct field has a type size associated to it
 *
 * @param[in] field_ptr struct field pointer
 * @return bool whether it is the case
 */
bool struct_field_has_typesize(const uint8_t *const field_ptr) {
    return (get_struct_field_typedesc(field_ptr) & TYPESIZE_MASK);
}

/**
 * Get type from a struct field
 *
 * @param[in] field_ptr struct field pointer
 * @return its type enum
 */
e_type struct_field_type(const uint8_t *const field_ptr) {
    return (get_struct_field_typedesc(field_ptr) & TYPE_MASK);
}

/**
 * Get type size from a struct field
 *
 * @param[in] field_ptr struct field pointer
 * @return its type size
 */
uint8_t get_struct_field_typesize(const uint8_t *const field_ptr) {
    if (field_ptr == NULL) {
        return 0;
    }
    return *field_skip_typedesc(field_ptr, NULL);
}

/**
 * Get custom type name from a struct field
 *
 * @param[in] field_ptr struct field pointer
 * @param[out] length the type name length
 * @return type name pointer
 */
const char *get_struct_field_custom_typename(const uint8_t *field_ptr, uint8_t *const length) {
    const uint8_t *ptr;

    if (field_ptr == NULL) {
        return NULL;
    }
    ptr = field_skip_typedesc(field_ptr, NULL);
    return get_string_in_mem(ptr, length);
}

/**
 * Get type name from a struct field
 *
 * @param[in] field_ptr struct field pointer
 * @param[out] length the type name length
 * @return type name pointer
 */
const char *get_struct_field_typename(const uint8_t *field_ptr, uint8_t *const length) {
    if (field_ptr == NULL) {
        return NULL;
    }
    if (struct_field_type(field_ptr) == TYPE_CUSTOM) {
        return get_struct_field_custom_typename(field_ptr, length);
    }
    return get_struct_field_sol_typename(field_ptr, length);
}

/**
 * Get array type of a given struct field's array depth
 *
 * @param[in] array_depth_ptr given array depth
 * @param[out] array_size pointer to array size
 * @return array type of that depth
 */
e_array_type struct_field_array_depth(const uint8_t *array_depth_ptr, uint8_t *const array_size) {
    if (array_depth_ptr == NULL) {
        return 0;
    }
    if (*array_depth_ptr == ARRAY_FIXED_SIZE) {
        if (array_size != NULL) {
            *array_size = *(array_depth_ptr + sizeof(uint8_t));
        }
    }
    return *array_depth_ptr;
}

/**
 * Get next array depth form a given struct field's array depth
 *
 * @param[in] array_depth_ptr given array depth
 * @return next array depth
 */
const uint8_t *get_next_struct_field_array_lvl(const uint8_t *const array_depth_ptr) {
    const uint8_t *ptr;

    if (array_depth_ptr == NULL) {
        return NULL;
    }
    switch (*array_depth_ptr) {
        case ARRAY_DYNAMIC:
            ptr = array_depth_ptr;
            break;
        case ARRAY_FIXED_SIZE:
            ptr = array_depth_ptr + 1;
            break;
        default:
            // should not be in here :^)
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return NULL;
    }
    return ptr + 1;
}

/**
 * Get the array levels from a given struct field
 *
 * @param[in] field_ptr given struct field
 * @param[out] length number of array levels
 * @return pointer to the first array level
 */
const uint8_t *get_struct_field_array_lvls_array(const uint8_t *const field_ptr,
                                                 uint8_t *const length) {
    const uint8_t *ptr;

    if (field_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return NULL;
    }
    ptr = field_skip_typedesc(field_ptr, NULL);
    ptr = field_skip_typename(field_ptr, ptr);
    ptr = field_skip_typesize(field_ptr, ptr);
    return get_array_in_mem(ptr, length);
}

/**
 * Get key name from a given struct field
 *
 * @param[in] field_ptr given struct field
 * @param[out] length name length
 * @return key name
 */
const char *get_struct_field_keyname(const uint8_t *field_ptr, uint8_t *const length) {
    const uint8_t *ptr;

    if (field_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return NULL;
    }
    ptr = field_skip_typedesc(field_ptr, NULL);
    ptr = field_skip_typename(field_ptr, ptr);
    ptr = field_skip_typesize(field_ptr, ptr);
    ptr = field_skip_array_levels(field_ptr, ptr);
    return get_string_in_mem(ptr, length);
}

/**
 * Get next struct field from a given field
 *
 * @param[in] field_ptr given struct field
 * @return pointer to the next field
 */
const uint8_t *get_next_struct_field(const void *const field_ptr) {
    const void *ptr;

    if (field_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return NULL;
    }
    ptr = field_skip_typedesc(field_ptr, NULL);
    ptr = field_skip_typename(field_ptr, ptr);
    ptr = field_skip_typesize(field_ptr, ptr);
    ptr = field_skip_array_levels(field_ptr, ptr);
    return field_skip_keyname(field_ptr, ptr);
}

/**
 * Get name from a given struct
 *
 * @param[in] struct_ptr given struct
 * @param[out] length name length
 * @return struct name
 */
const char *get_struct_name(const uint8_t *const struct_ptr, uint8_t *const length) {
    if (struct_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return NULL;
    }
    return (char *) get_string_in_mem(struct_ptr, length);
}

/**
 * Get struct fields from a given struct
 *
 * @param[in] struct_ptr given struct
 * @param[out] length number of fields
 * @return struct name
 */
const uint8_t *get_struct_fields_array(const uint8_t *const struct_ptr, uint8_t *const length) {
    const void *ptr;
    uint8_t name_length;

    if (struct_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return NULL;
    }
    ptr = struct_ptr;
    get_struct_name(struct_ptr, &name_length);
    ptr += (sizeof(name_length) + name_length);  // skip length
    return get_array_in_mem(ptr, length);
}

/**
 * Get next struct from a given struct
 *
 * @param[in] struct_ptr given struct
 * @return pointer to next struct
 */
const uint8_t *get_next_struct(const uint8_t *const struct_ptr) {
    uint8_t fields_count;
    const void *ptr;

    if (struct_ptr == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return NULL;
    }
    ptr = get_struct_fields_array(struct_ptr, &fields_count);
    while (fields_count-- > 0) {
        ptr = get_next_struct_field(ptr);
    }
    return ptr;
}

/**
 * Get structs array
 *
 * @param[out] length number of structs
 * @return pointer to the first struct
 */
const uint8_t *get_structs_array(uint8_t *const length) {
    return get_array_in_mem(typed_data->structs_array, length);
}

/**
 * Find struct with a given name
 *
 * @param[in] name struct name
 * @param[in] length name length
 * @return pointer to struct
 */
const uint8_t *get_structn(const char *const name, const uint8_t length) {
    uint8_t structs_count = 0;
    const uint8_t *struct_ptr;
    const char *struct_name;
    uint8_t name_length;

    if (name == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return NULL;
    }
    struct_ptr = get_structs_array(&structs_count);
    while (structs_count-- > 0) {
        struct_name = get_struct_name(struct_ptr, &name_length);
        if ((length == name_length) && (memcmp(name, struct_name, length) == 0)) {
            return struct_ptr;
        }
        struct_ptr = get_next_struct(struct_ptr);
    }
    apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    return NULL;
}

/**
 * Set struct name
 *
 * @param[in] length name length
 * @param[in] name name
 * @return whether it was successful
 */
bool set_struct_name(uint8_t length, const uint8_t *const name) {
    uint8_t *length_ptr;
    char *name_ptr;

    if ((name == NULL) || (typed_data == NULL)) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    // increment number of structs
    if ((*(typed_data->structs_array) += 1) == 0) {
        PRINTF("EIP712 Structs count overflow!\n");
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    // copy length
    if ((length_ptr = mem_alloc(sizeof(uint8_t))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *length_ptr = length;

    // copy name
    if ((name_ptr = mem_alloc(sizeof(char) * length)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    memmove(name_ptr, name, length);

    // initialize number of fields
    if ((typed_data->current_struct_fields_array = mem_alloc(sizeof(uint8_t))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *(typed_data->current_struct_fields_array) = 0;

    struct_state = INITIALIZED;
    return true;
}

/**
 * Set struct field TypeDesc
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return pointer to the TypeDesc in memory
 */
static const typedesc_t *set_struct_field_typedesc(const uint8_t *const data,
                                                   uint8_t *data_idx,
                                                   uint8_t length) {
    typedesc_t *typedesc_ptr;

    // copy TypeDesc
    if ((*data_idx + sizeof(*typedesc_ptr)) > length)  // check buffer bound
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return NULL;
    }
    if ((typedesc_ptr = mem_alloc(sizeof(uint8_t))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return NULL;
    }
    *typedesc_ptr = data[(*data_idx)++];
    return typedesc_ptr;
}

/**
 * Set struct field custom typename
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_custom_typename(const uint8_t *const data,
                                             uint8_t *data_idx,
                                             uint8_t length) {
    uint8_t *typename_len_ptr;
    char *typename;

    // copy custom struct name length
    if ((*data_idx + sizeof(*typename_len_ptr)) > length)  // check buffer bound
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if ((typename_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *typename_len_ptr = data[(*data_idx)++];

    // copy name
    if ((*data_idx + *typename_len_ptr) > length)  // check buffer bound
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if ((typename = mem_alloc(sizeof(char) * *typename_len_ptr)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    memmove(typename, &data[*data_idx], *typename_len_ptr);
    *data_idx += *typename_len_ptr;
    return true;
}

/**
 * Set struct field's array levels
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_array(const uint8_t *const data, uint8_t *data_idx, uint8_t length) {
    uint8_t *array_levels_count;
    uint8_t *array_level;
    uint8_t *array_level_size;

    if ((*data_idx + sizeof(*array_levels_count)) > length)  // check buffer bound
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if ((array_levels_count = mem_alloc(sizeof(uint8_t))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *array_levels_count = data[(*data_idx)++];
    for (int idx = 0; idx < *array_levels_count; ++idx) {
        if ((*data_idx + sizeof(*array_level)) > length)  // check buffer bound
        {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
        }
        if ((array_level = mem_alloc(sizeof(*array_level))) == NULL) {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }
        *array_level = data[(*data_idx)++];
        if (*array_level >= ARRAY_TYPES_COUNT) {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
        }
        switch (*array_level) {
            case ARRAY_DYNAMIC:  // nothing to do
                break;
            case ARRAY_FIXED_SIZE:
                if ((*data_idx + sizeof(*array_level_size)) > length)  // check buffer bound
                {
                    apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                    return false;
                }
                if ((array_level_size = mem_alloc(sizeof(uint8_t))) == NULL) {
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

/**
 * Set struct field's type size
 *
 * @param[in] data the field data
 * @param[in,out] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_typesize(const uint8_t *const data,
                                      uint8_t *data_idx,
                                      uint8_t length) {
    uint8_t *typesize_ptr;

    // copy TypeSize
    if ((*data_idx + sizeof(*typesize_ptr)) > length)  // check buffer bound
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if ((typesize_ptr = mem_alloc(sizeof(uint8_t))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *typesize_ptr = data[(*data_idx)++];
    return true;
}

/**
 * Set struct field's key name
 *
 * @param[in] data the field data
 * @param[in,out] data_idx the data index
 * @return whether it was successful
 */
static bool set_struct_field_keyname(const uint8_t *const data, uint8_t *data_idx, uint8_t length) {
    uint8_t *keyname_len_ptr;
    char *keyname_ptr;

    // copy length
    if ((*data_idx + sizeof(*keyname_len_ptr)) > length)  // check buffer bound
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if ((keyname_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    *keyname_len_ptr = data[(*data_idx)++];

    // copy name
    if ((*data_idx + *keyname_len_ptr) > length)  // check buffer bound
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if ((keyname_ptr = mem_alloc(sizeof(char) * *keyname_len_ptr)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    memmove(keyname_ptr, &data[*data_idx], *keyname_len_ptr);
    *data_idx += *keyname_len_ptr;
    return true;
}

/**
 * Set struct field
 *
 * @param[in] length data length
 * @param[in] data the field data
 * @return whether it was successful
 */
bool set_struct_field(uint8_t length, const uint8_t *const data) {
    const typedesc_t *typedesc_ptr;
    uint8_t data_idx = 0;

    if ((data == NULL) || (length == 0)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    } else if (typed_data == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    if (struct_state == NOT_INITIALIZED) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    // increment number of struct fields
    if ((*(typed_data->current_struct_fields_array) += 1) == 0) {
        PRINTF("EIP712 Struct fields count overflow!\n");
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    if ((typedesc_ptr = set_struct_field_typedesc(data, &data_idx, length)) == NULL) {
        return false;
    }

    // check TypeSize flag in TypeDesc
    if (*typedesc_ptr & TYPESIZE_MASK) {
        // TYPESIZE and TYPE_CUSTOM are mutually exclusive
        if ((*typedesc_ptr & TYPE_MASK) == TYPE_CUSTOM) {
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return false;
        }

        if (set_struct_field_typesize(data, &data_idx, length) == false) {
            return false;
        }

    } else if ((*typedesc_ptr & TYPE_MASK) == TYPE_CUSTOM) {
        if (set_struct_field_custom_typename(data, &data_idx, length) == false) {
            return false;
        }
    }
    if (*typedesc_ptr & ARRAY_MASK) {
        if (set_struct_field_array(data, &data_idx, length) == false) {
            return false;
        }
    }

    if (set_struct_field_keyname(data, &data_idx, length) == false) {
        return false;
    }

    if (data_idx != length)  // check that there is no more
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    return true;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
