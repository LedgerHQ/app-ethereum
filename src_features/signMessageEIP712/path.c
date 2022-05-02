#include <stdint.h>
#include <string.h>
#include "path.h"
#include "mem.h"
#include "context.h"
#include "eip712.h"
#include "type_hash.h"
#include "shared_context.h"

static s_path *path_struct = NULL;

/**
 * Get the field pointer to by the first N depths of the path.
 *
 * @param[out] fields_count_ptr the number of fields in the last evaluated depth
 * @param[in] depth_count the number of depths to evaluate (N)
 * @return the feld which the first Nth depths points to
 */
static const void *get_nth_field_from_path(uint8_t *const fields_count_ptr,
                                           uint8_t depth_count)
{
    const void *struct_ptr = path_struct->root_struct;
    const void *field_ptr = NULL;
    const char *typename;
    uint8_t length;
    uint8_t fields_count;

    if (depth_count > path_struct->depth_count) // sanity check
    {
        return NULL;
    }
    for (uint8_t depth = 0; depth < depth_count; ++depth)
    {
        field_ptr = get_struct_fields_array(struct_ptr, &fields_count);

        if (fields_count_ptr != NULL)
        {
            *fields_count_ptr = fields_count;
        }
        // check if the index at this depth makes sense
        if (path_struct->depths[depth] > fields_count)
        {
            return NULL;
        }

        for (uint8_t index = 0; index < path_struct->depths[depth]; ++index)
        {
            field_ptr = get_next_struct_field(field_ptr);
        }
        if (struct_field_type(field_ptr) == TYPE_CUSTOM)
        {
            typename = get_struct_field_typename(field_ptr, &length);
            if ((struct_ptr = get_structn(structs_array, typename, length)) == NULL)
            {
                return NULL;
            }
        }
    }
    return field_ptr;
}

/**
 * Get the element the path is pointing to. (internal)
 *
 * @param[out] the number of fields in the depth of the returned field
 * @return the field which the path points to
 */
static inline const void  *get_field_from_path(uint8_t *const fields_count)
{
    return get_nth_field_from_path(fields_count, path_struct->depth_count);
}

/**
 * Get the element the path is pointing to. (public facing)
 *
 * @return the field which the path points to
 */
const void *path_get_field(void)
{
    return get_field_from_path(NULL);
}

/**
 * Go down (add) a depth level.
 *
 * @return whether the push was succesful
 */
static bool path_depth_list_push(void)
{
    if (path_struct->depth_count == MAX_PATH_DEPTH)
    {
        return false;
    }
    path_struct->depths[path_struct->depth_count] = 0;
    path_struct->depth_count += 1;
    return true;
}

/**
 * Go up (remove) a depth level.
 *
 * @return whether the pop was successful
 */
static bool path_depth_list_pop(void)
{
    if (path_struct->depth_count == 0)
    {
        return false;
    }
    path_struct->depth_count -= 1;

    // TODO: Move elsewhere
    uint8_t shash[KECCAK256_HASH_BYTESIZE];
    cx_sha3_t *hash_ctx = mem_alloc(0) - sizeof(cx_sha3_t);
    // finalize hash
    cx_hash((cx_hash_t*)hash_ctx,
            CX_LAST,
            NULL,
            0,
            &shash[0],
            KECCAK256_HASH_BYTESIZE);
    mem_dealloc(sizeof(cx_sha3_t)); // remove hash context
#ifdef DEBUG
    // print computed hash
    PRINTF("SHASH POP 0x");
    for (int idx = 0; idx < KECCAK256_HASH_BYTESIZE; ++idx)
    {
        PRINTF("%.02x", shash[idx]);
    }
    PRINTF("\n");
#endif
    if (path_struct->depth_count > 0)
    {
        hash_ctx = mem_alloc(0) - sizeof(cx_sha3_t); // previous one
        // continue progressive hash with the array hash
        cx_hash((cx_hash_t*)hash_ctx,
                0,
                &shash[0],
                KECCAK256_HASH_BYTESIZE,
                NULL,
                0);
    }
    else
    {
        PRINTF("\n");
    }

    return true;
}

/**
 * Go down (add) an array depth level.
 *
 * @param[in] path_idx the index in the path list
 * @param[in] the number of elements contained in that depth
 * @return whether the push was successful
 */
static bool array_depth_list_push(uint8_t path_idx, uint8_t size)
{
    s_array_depth *arr = &path_struct->array_depths[path_struct->array_depth_count];

    if (path_struct->array_depth_count == MAX_ARRAY_DEPTH)
    {
        return false;
    }

    arr->path_index = path_idx;
    arr->size = size;
    path_struct->array_depth_count += 1;
    return true;
}

/**
 * Go up (remove) an array depth level.
 *
 * @return whether the pop was successful
 */
static bool array_depth_list_pop(void)
{
    if (path_struct->array_depth_count == 0)
    {
        return false;
    }

    // TODO: Move elsewhere
    uint8_t ahash[KECCAK256_HASH_BYTESIZE];
    cx_sha3_t *hash_ctx = mem_alloc(0) - sizeof(cx_sha3_t);
    // finalize hash
    cx_hash((cx_hash_t*)hash_ctx,
            CX_LAST,
            NULL,
            0,
            &ahash[0],
            KECCAK256_HASH_BYTESIZE);
    mem_dealloc(sizeof(cx_sha3_t)); // remove hash context
    hash_ctx = mem_alloc(0) - sizeof(cx_sha3_t); // previous one
    // continue progressive hash with the array hash
    cx_hash((cx_hash_t*)hash_ctx,
            0,
            &ahash[0],
            KECCAK256_HASH_BYTESIZE,
            NULL,
            0);
    PRINTF("AHASH POP\n");

    path_struct->array_depth_count -= 1;
    return true;
}

/**
 * Updates the path so that it doesn't point to a struct-type field, but rather
 * only to actual fields.
 *
 * @return whether the path update worked or not
 */
static bool path_update(void)
{
    uint8_t fields_count;
    const void *struct_ptr = path_struct->root_struct;
    const void *field_ptr;
    const char *typename;
    uint8_t typename_len;

    if ((field_ptr = get_field_from_path(NULL)) == NULL)
    {
        return false;
    }
    while (struct_field_type(field_ptr) == TYPE_CUSTOM)
    {
        typename = get_struct_field_typename(field_ptr, &typename_len);
        if ((struct_ptr = get_structn(structs_array, typename, typename_len)) == NULL)
        {
            return false;
        }
        if ((field_ptr = get_struct_fields_array(struct_ptr, &fields_count)) == NULL)
        {
            return false;
        }

        // TODO: Move elsewhere
        cx_sha3_t *hash_ctx;
        const uint8_t *thash_ptr;

        // allocate new hash context
        if ((hash_ctx = mem_alloc(sizeof(cx_sha3_t))) == NULL)
        {
            return false;
        }
        cx_keccak_init((cx_hash_t*)hash_ctx, 256); // initialize it
        // get the struct typehash
        if ((thash_ptr = type_hash(structs_array, typename, typename_len, true)) == NULL)
        {
            return false;
        }
        // start the progressive hash on it
        cx_hash((cx_hash_t*)hash_ctx,
                0,
                thash_ptr,
                KECCAK256_HASH_BYTESIZE,
                NULL,
                0);
        // deallocate it
        mem_dealloc(KECCAK256_HASH_BYTESIZE);
        PRINTF("SHASH PUSH w/o deps %p\n", hash_ctx);

        path_depth_list_push();
    }
    return true;
}

/**
 * Set a new struct as the path root type
 *
 * @param[in] struct_name the root struct name
 * @param[in] name_length the root struct name length
 * @return boolean indicating if it was successful or not
 */
bool    path_set_root(const char *const struct_name, uint8_t name_length)
{
    if (path_struct == NULL)
    {
        return false;
    }

    path_struct->root_struct = get_structn(structs_array, struct_name, name_length);

    if (path_struct->root_struct == NULL)
    {
        return false;
    }

    // TODO: Move elsewhere
    cx_sha3_t *hash_ctx;
    const uint8_t *thash_ptr;
    if ((hash_ctx = mem_alloc(sizeof(cx_sha3_t))) == NULL)
    {
        return false;
    }
    cx_keccak_init((cx_hash_t*)hash_ctx, 256); // init hash
    if ((thash_ptr = type_hash(structs_array, struct_name, name_length, true)) == NULL)
    {
        return false;
    }
    // start the progressive hash on it
    cx_hash((cx_hash_t*)hash_ctx,
            0,
            thash_ptr,
            KECCAK256_HASH_BYTESIZE,
            NULL,
            0);
    // deallocate it
    mem_dealloc(KECCAK256_HASH_BYTESIZE);
    PRINTF("SHASH PUSH w/ deps %p\n", hash_ctx);
    //

    // init depth, at 0 : empty path
    path_struct->depth_count = 0;
    path_depth_list_push();

    // init array levels at 0
    path_struct->array_depth_count = 0;

    // because the first field could be a struct type
    path_update();
    return true;
}

/**
 * Checks the new array depth and adds it to the list
 *
 * @param[in] depth pointer to the array depth definition
 * @param[in] total_count number of array depth contained down to this array depth
 * @param[in] pidx path index
 * @param[in] size requested array depth size
 * @return whether the checks and add were successful or not
 */
static bool check_and_add_array_depth(const void *depth,
                                 uint8_t total_count,
                                 uint8_t pidx,
                                 uint8_t size)
{
    uint8_t expected_size;
    uint8_t arr_idx = (total_count - path_struct->array_depth_count) - 1;
    e_array_type expected_type;

    // we skip index 0, since we already have it
    for (uint8_t idx = 1; idx < (arr_idx + 1); ++idx)
    {
        if ((depth = get_next_struct_field_array_lvl(depth)) == NULL)
        {
            return false;
        }
    }
    expected_type = struct_field_array_depth(depth, &expected_size);
    if ((expected_type == ARRAY_FIXED_SIZE) && (expected_size != size))
    {
        PRINTF("Unexpected array depth size. (expected %d, got %d)\n",
               expected_size, size);
        return false;
    }
    // add it
    if (!array_depth_list_push(pidx, size))
    {
        return false;
    }
    return true;
}

/**
 * Add a new array depth with a given size (number of elements).
 *
 * @return whether the add was successful or not
 */
bool    path_new_array_depth(uint8_t size)
{
    const void *field_ptr = NULL;
    const void *depth = NULL;
    uint8_t depth_count;
    uint8_t total_count = 0;
    uint8_t pidx;

    if (path_struct == NULL) // sanity check
    {
        PRINTF("NULL struct check failed\n");
        return false;
    }

    for (pidx = 0; pidx < path_struct->depth_count; ++pidx)
    {
        if ((field_ptr = get_nth_field_from_path(NULL, pidx + 1)) == NULL)
        {
            return false;
        }
        if (struct_field_is_array(field_ptr))
        {
            if ((depth = get_struct_field_array_lvls_array(field_ptr, &depth_count)) == NULL)
            {
                return false;
            }
            total_count += depth_count;
            if (total_count > path_struct->array_depth_count)
            {
                if (!check_and_add_array_depth(depth, total_count, pidx, size))
                {
                    return false;
                }
                break;
            }
        }
    }

    if (pidx == path_struct->depth_count)
    {
        PRINTF("Did not find a matching array type.\n");
        return false;
    }
    // TODO: Move elsewhere
    cx_sha3_t *hash_ctx;
    if ((hash_ctx = mem_alloc(sizeof(cx_sha3_t))) == NULL)
    {
        return false;
    }
    PRINTF("AHASH PUSH %p", hash_ctx);
    if (struct_field_type(field_ptr) == TYPE_CUSTOM)
    {
        cx_sha3_t *old_ctx = (void*)hash_ctx - sizeof(cx_sha3_t);

        memcpy(hash_ctx, old_ctx, sizeof(cx_sha3_t));
        cx_keccak_init((cx_hash_t*)old_ctx, 256); // init hash
        PRINTF(" (switched)");
    }
    else // solidity type
    {
        cx_keccak_init((cx_hash_t*)hash_ctx, 256); // init hash
    }
    PRINTF("\n");

    return true;
}

/**
 * Advance within the struct that contains the field the path points to.
 *
 * @return whether the end of the struct has been reached.
 */
static bool path_advance_in_struct(void)
{
    bool end_reached = true;
    uint8_t *depth = &path_struct->depths[path_struct->depth_count - 1];
    uint8_t fields_count;

    if ((get_field_from_path(&fields_count)) == NULL)
    {
        return false;
    }
    if (path_struct->depth_count > 0)
    {
        *depth += 1;
        end_reached = (*depth == fields_count);
    }
    if (end_reached)
    {
        path_depth_list_pop();
    }
    return end_reached;
}

/**
 * Advance within the array levels of the current field the path points to.
 *
 * @return whether the end of the array levels has been reached.
 */
static bool path_advance_in_array(void)
{
    bool end_reached;
    s_array_depth *arr_depth;

    do
    {
        end_reached = false;
        arr_depth = &path_struct->array_depths[path_struct->array_depth_count - 1];

        if ((path_struct->array_depth_count > 0) &&
            (arr_depth->path_index == (path_struct->depth_count - 1)))
        {
            arr_depth->size -= 1;
            if (arr_depth->size == 0)
            {
                array_depth_list_pop();
                end_reached = true;
            }
            else
            {
                return false;
            }
        }
    }
    while (end_reached);
    return true;
}

/**
 * Updates the path to point to the next field in order (DFS).
 *
 * @return whether the advancement was successful or not
 */
bool    path_advance(void)
{
    bool end_reached;

    do
    {
        if (path_advance_in_array())
        {
            end_reached = path_advance_in_struct();
        }
        else
        {
            end_reached = false;
        }
    }
    while (end_reached);
    path_update();
    return true;
}

/**
 * Allocates the the path indexes in memory and sets it with a depth of 0.
 *
 * @return whether the memory allocation were successful.
 */
bool    path_init(void)
{
    if (path_struct == NULL)
    {
        path_struct = mem_alloc(sizeof(*path_struct));
    }
    return path_struct != NULL;
}
