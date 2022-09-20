#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "mem.h"
#include "mem_utils.h"
#include "eip712.h"
#include "type_hash.h"
#include "shared_context.h"
#include "ethUtils.h" // KECCAK256_HASH_BYTESIZE
#include "format_hash_field_type.h"
#include "hash_bytes.h"

/**
 *
 * @param[in] field_ptr pointer to the struct field
 * @return \ref true it finished correctly, \ref false if it didn't (memory allocation)
 */
static bool encode_and_hash_field(const void *const field_ptr)
{
    const char *name;
    uint8_t length;

    if (!format_hash_field_type(field_ptr, (cx_hash_t*)&global_sha3))
    {
        return false;
    }
    // space between field type name and field name
    hash_byte(' ', (cx_hash_t*)&global_sha3);

    // field name
    name = get_struct_field_keyname(field_ptr, &length);
    hash_nbytes((uint8_t*)name, length, (cx_hash_t*)&global_sha3);
    return true;
}

/**
 *
 * @param[in] struct_ptr pointer to the structure we want the typestring of
 * @param[in] str_length length of the formatted string in memory
 * @return pointer of the string in memory, \ref NULL in case of an error
 */
static bool encode_and_hash_type(const void *const struct_ptr)
{
    const char *struct_name;
    uint8_t struct_name_length;
    const uint8_t *field_ptr;
    uint8_t fields_count;

    // struct name
    struct_name = get_struct_name(struct_ptr, &struct_name_length);
    hash_nbytes((uint8_t*)struct_name, struct_name_length, (cx_hash_t*)&global_sha3);

    // opening struct parenthese
    hash_byte('(', (cx_hash_t*)&global_sha3);

    field_ptr = get_struct_fields_array(struct_ptr, &fields_count);
    for (uint8_t idx = 0; idx < fields_count; ++idx)
    {
        // comma separating struct fields
        if (idx > 0)
        {
            hash_byte(',', (cx_hash_t*)&global_sha3);
        }

        if (encode_and_hash_field(field_ptr) == false)
        {
            return NULL;
        }

        field_ptr = get_next_struct_field(field_ptr);
    }
    // closing struct parenthese
    hash_byte(')', (cx_hash_t*)&global_sha3);

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
    void *mem_loc_bak = mem_alloc(0);

    cx_keccak_init(&global_sha3, 256); // init hash
    // get list of structs (own + dependencies), properly ordered
    if ((deps = MEM_ALLOC_AND_ALIGN_TO_TYPE(0, *deps)) == NULL)
    {
        return NULL;
    }
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
    mem_dealloc(mem_alloc(0) - mem_loc_bak);

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

#endif // HAVE_EIP712_FULL_SUPPORT
