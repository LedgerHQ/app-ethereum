#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "mem.h"
#include "mem_utils.h"
#include "eip712.h"
#include "type_hash.h"
#include "shared_context.h"

static inline void hash_nbytes(const uint8_t *b, uint8_t n)
{
#ifdef DEBUG
    for (int i = 0; i < n; ++i)
    {
        PRINTF("%c", b[i]);
    }
#endif
    cx_hash((cx_hash_t*)&global_sha3,
            0,
            b,
            n,
            NULL,
            0);
}

static inline void hash_byte(uint8_t b)
{
    hash_nbytes(&b, 1);
}

/**
 *
 * @param[in] lvl_ptr pointer to the first array level of a struct field
 * @param[in] lvls_count the number of array levels the struct field contains
 * @return \ref true it finished correctly, \ref false if it didn't (memory allocation)
 */
static bool format_field_type_array_levels_string(const void *lvl_ptr, uint8_t lvls_count)
{
    uint8_t array_size;
    char *uint_str_ptr;
    uint8_t uint_str_len;

    while (lvls_count-- > 0)
    {
        hash_byte('[');

        switch (struct_field_array_depth(lvl_ptr, &array_size))
        {
            case ARRAY_DYNAMIC:
                break;
            case ARRAY_FIXED_SIZE:
                uint_str_ptr = mem_alloc_and_format_uint(array_size, &uint_str_len);
                hash_nbytes((uint8_t*)uint_str_ptr, uint_str_len);
                mem_dealloc(uint_str_len);
                break;
            default:
                // should not be in here :^)
                break;
        }
        hash_byte(']');
        lvl_ptr = get_next_struct_field_array_lvl(lvl_ptr);
    }
    return true;
}

/**
 *
 * @param[in] field_ptr pointer to the struct field
 * @return \ref true it finished correctly, \ref false if it didn't (memory allocation)
 */
static bool encode_and_hash_field(const void *field_ptr)
{
    const char *name;
    uint8_t length;
    uint16_t field_size;
    uint8_t lvls_count;
    const uint8_t *lvl_ptr;
    char *uint_str_ptr;
    uint8_t uint_str_len;

    // field type name
    name = get_struct_field_typename(field_ptr, &length);
    hash_nbytes((uint8_t*)name, length);

    // field type size
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
        uint_str_ptr = mem_alloc_and_format_uint(field_size, &uint_str_len);
        hash_nbytes((uint8_t*)uint_str_ptr, uint_str_len);
        mem_dealloc(uint_str_len);
    }

    // field type array levels
    if (struct_field_is_array(field_ptr))
    {
        lvl_ptr = get_struct_field_array_lvls_array(field_ptr, &lvls_count);
        format_field_type_array_levels_string(lvl_ptr, lvls_count);
    }
    // space between field type name and field name
    hash_byte(' ');

    // field name
    name = get_struct_field_keyname(field_ptr, &length);
    hash_nbytes((uint8_t*)name, length);
    return true;
}

/**
 *
 * @param[in] struct_ptr pointer to the structure we want the typestring of
 * @param[in] str_length length of the formatted string in memory
 * @return pointer of the string in memory, \ref NULL in case of an error
 */
static bool encode_and_hash_type(const uint8_t *const struct_ptr)
{
    const char *struct_name;
    uint8_t struct_name_length;
    const uint8_t *field_ptr;
    uint8_t fields_count;

    // struct name
    struct_name = get_struct_name(struct_ptr, &struct_name_length);
    hash_nbytes((uint8_t*)struct_name, struct_name_length);

    // opening struct parenthese
    hash_byte('(');

    field_ptr = get_struct_fields_array(struct_ptr, &fields_count);
    for (uint8_t idx = 0; idx < fields_count; ++idx)
    {
        // comma separating struct fields
        if (idx > 0)
        {
            hash_byte(',');
        }

        if (encode_and_hash_field(field_ptr) == false)
        {
            return NULL;
        }

        field_ptr = get_next_struct_field(field_ptr);
    }
    // closing struct parenthese
    hash_byte(')');

    return true;
}

/**
 *
 *
 * @param[in] structs_array pointer to structs array
 * @param[in] deps_count count of how many struct dependencies pointers
 * @param[in,out] deps pointer to the first dependency pointer
 */
static void sort_dependencies(uint8_t deps_count,
                              void **deps)
{
    bool changed;
    void *tmp_ptr;
    const char *name1, *name2;
    uint8_t namelen1, namelen2;
    int str_cmp_result;

    do
    {
        changed = false;
        for (size_t idx = 0; (idx + 1) < deps_count; ++idx)
        {
            name1 = get_struct_name(*(deps + idx), &namelen1);
            name2 = get_struct_name(*(deps + idx + 1), &namelen2);

            str_cmp_result = strncmp(name1, name2, MIN(namelen1, namelen2));
            if ((str_cmp_result > 0) || ((str_cmp_result == 0) && (namelen1 > namelen2)))
            {
                tmp_ptr = *(deps + idx);
                *(deps + idx) = *(deps + idx + 1);
                *(deps + idx + 1) = tmp_ptr;

                changed = true;
            }
        }
    }
    while (changed);
}

/**
 *
 *
 * @param[in] structs_array pointer to structs array
 * @param[out] deps_count count of how many struct dependencie pointers
 * @param[in] deps pointer to the first dependency pointer
 * @param[in] struct_ptr pointer to the struct we are getting the dependencies of
 * @return \ref false in case of a memory allocation error, \ref true otherwise
 */
static bool get_struct_dependencies(const void *const structs_array,
                                    uint8_t *const deps_count,
                                    void *const *const deps,
                                    const void *const struct_ptr)
{
    uint8_t fields_count;
    const void *field_ptr;
    const char *arg_structname;
    uint8_t arg_structname_length;
    const void *arg_struct_ptr;
    size_t dep_idx;
    const void **new_dep;

    field_ptr = get_struct_fields_array(struct_ptr, &fields_count);
    for (uint8_t idx = 0; idx < fields_count; ++idx)
    {
        if (struct_field_type(field_ptr) == TYPE_CUSTOM)
        {
            // get struct name
            arg_structname = get_struct_field_typename(field_ptr, &arg_structname_length);
            // from its name, get the pointer to its definition
            arg_struct_ptr = get_structn(structs_array, arg_structname, arg_structname_length);

            // check if it is not already present in the dependencies array
            for (dep_idx = 0; dep_idx < *deps_count; ++dep_idx)
            {
                // it's a match!
                if (*(deps + dep_idx) == arg_struct_ptr)
                {
                    break;
                }
            }
            // if it's not present in the array, add it and recurse into it
            if (dep_idx == *deps_count)
            {
                if ((new_dep = mem_alloc(sizeof(void*))) == NULL)
                {
                    return false;
                }
                *new_dep = arg_struct_ptr;
                *deps_count += 1;
                get_struct_dependencies(structs_array, deps_count, deps, arg_struct_ptr);
            }
        }
        field_ptr = get_next_struct_field(field_ptr);
    }
    return true;
}

/**
 *
 *
 * @param[in] structs_array pointer to structs array
 * @param[in] struct_name name of the given struct
 * @param[in] struct_name_length length of the name of the given struct
 * @param[in] with_deps if hashed typestring should include struct dependencies
 * @return pointer to encoded string or \ref NULL in case of a memory allocation error
 */
const uint8_t *type_hash(const void *const structs_array,
                         const char *const struct_name,
                         const uint8_t struct_name_length)
{
    const void *const struct_ptr = get_structn(structs_array,
                                               struct_name,
                                               struct_name_length);
    uint8_t deps_count = 0;
    void **deps;
    uint8_t *hash_ptr;

    cx_keccak_init((cx_hash_t*)&global_sha3, 256); // init hash
    // get list of structs (own + dependencies), properly ordered
    deps = mem_alloc(0); // get where the first elem will be
    if (get_struct_dependencies(structs_array, &deps_count, deps, struct_ptr) == false)
    {
        return NULL;
    }
    sort_dependencies(deps_count, deps);
    if (encode_and_hash_type(struct_ptr) == false)
    {
        return NULL;
    }
    // loop over each struct and generate string
    for (int idx = 0; idx < deps_count; ++idx)
    {
        encode_and_hash_type(*deps);
        deps += 1;
    }
    mem_dealloc(sizeof(void*) * deps_count);
#ifdef DEBUG
    PRINTF("\n");
#endif
    // End progressive hashing
    if ((hash_ptr = mem_alloc(KECCAK256_HASH_BYTESIZE)) == NULL)
    {
        return NULL;
    }
    // copy hash into memory
    cx_hash((cx_hash_t*)&global_sha3,
            CX_LAST,
            NULL,
            0,
            hash_ptr,
            KECCAK256_HASH_BYTESIZE);
    return hash_ptr;
}
