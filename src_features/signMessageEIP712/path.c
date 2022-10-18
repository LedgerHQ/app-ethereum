#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>
#include <string.h>
#include "path.h"
#include "mem.h"
#include "context_712.h"
#include "commands_712.h"
#include "type_hash.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "mem_utils.h"
#include "ui_logic.h"
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"

static s_path *path_struct = NULL;

/**
 * Get the field pointer to by the first N depths of the path.
 *
 * @param[out] fields_count_ptr the number of fields in the last evaluated depth
 * @param[in] n the number of depths to evaluate
 * @return the field which the first Nth depths points to
 */
static const void *get_nth_field(uint8_t *const fields_count_ptr, uint8_t n) {
    const void *struct_ptr = NULL;
    const void *field_ptr = NULL;
    const char *typename;
    uint8_t length;
    uint8_t fields_count;

    if (path_struct == NULL) {
        return NULL;
    }

    struct_ptr = path_struct->root_struct;

    if (n > path_struct->depth_count)  // sanity check
    {
        return NULL;
    }
    for (uint8_t depth = 0; depth < n; ++depth) {
        field_ptr = get_struct_fields_array(struct_ptr, &fields_count);

        if (fields_count_ptr != NULL) {
            *fields_count_ptr = fields_count;
        }
        // check if the index at this depth makes sense
        if (path_struct->depths[depth] > fields_count) {
            return NULL;
        }

        for (uint8_t index = 0; index < path_struct->depths[depth]; ++index) {
            field_ptr = get_next_struct_field(field_ptr);
        }
        if (struct_field_type(field_ptr) == TYPE_CUSTOM) {
            typename = get_struct_field_typename(field_ptr, &length);
            if ((struct_ptr = get_structn(typename, length)) == NULL) {
                return NULL;
            }
        }
    }
    return field_ptr;
}

/**
 * Get the element the path is pointing to.
 *
 * @param[out] the number of fields in the depth of the returned field
 * @return the field which the path points to
 */
static inline const void *get_field(uint8_t *const fields_count) {
    return get_nth_field(fields_count, path_struct->depth_count);
}

/**
 * Get Nth struct field from path
 *
 * @param[in] n nth depth requested
 * @return pointer to the matching field, \ref NULL otherwise
 */
const void *path_get_nth_field(uint8_t n) {
    return get_nth_field(NULL, n);
}

/**
 * Get Nth to last struct field from path
 *
 * @param[in] n nth to last depth requested
 * @return pointer to the matching field, \ref NULL otherwise
 */
const void *path_get_nth_field_to_last(uint8_t n) {
    const char *typename;
    uint8_t typename_len;
    const void *field_ptr;
    const void *struct_ptr = NULL;

    field_ptr = get_nth_field(NULL, path_struct->depth_count - n);
    if (field_ptr != NULL) {
        typename = get_struct_field_typename(field_ptr, &typename_len);
        struct_ptr = get_structn(typename, typename_len);
    }
    return struct_ptr;
}

/**
 * Get the element the path is pointing to
 *
 * @return the field which the path points to
 */
const void *path_get_field(void) {
    return get_field(NULL);
}

/**
 * Go down (add) a depth level.
 *
 * @return whether the push was succesful
 */
static bool path_depth_list_push(void) {
    if (path_struct == NULL) {
        return false;
    }
    if (path_struct->depth_count == MAX_PATH_DEPTH) {
        return false;
    }
    path_struct->depths[path_struct->depth_count] = 0;
    path_struct->depth_count += 1;
    return true;
}

/**
 * Get the last hashing context (corresponding to the current path depth)
 *
 * @return pointer to the hashing context
 */
static cx_sha3_t *get_last_hash_ctx(void) {
    return (cx_sha3_t *) mem_alloc(0) - 1;
}

/**
 * Finalize the last hashing context
 *
 * @param[out] hash pointer to buffer where the hash will be stored
 */
static void finalize_hash_depth(uint8_t *hash) {
    const cx_sha3_t *hash_ctx;

    hash_ctx = get_last_hash_ctx();
    // finalize hash
    cx_hash((cx_hash_t *) hash_ctx, CX_LAST, NULL, 0, hash, KECCAK256_HASH_BYTESIZE);
    mem_dealloc(sizeof(*hash_ctx));  // remove hash context
}

/**
 * Continue last progressive hashing context with given hash
 *
 * @param[in] hash pointer to given hash
 */
static void feed_last_hash_depth(const uint8_t *const hash) {
    const cx_sha3_t *hash_ctx;

    hash_ctx = get_last_hash_ctx();
    // continue progressive hash with the array hash
    cx_hash((cx_hash_t *) hash_ctx, 0, hash, KECCAK256_HASH_BYTESIZE, NULL, 0);
}

/**
 * Create a new hashing context depth in memory
 *
 * @param[in] init if the hashing context should be initialized
 * @return whether the memory allocation of the hashing context was successful
 */
static bool push_new_hash_depth(bool init) {
    cx_sha3_t *hash_ctx;

    // allocate new hash context
    if ((hash_ctx = MEM_ALLOC_AND_ALIGN_TYPE(*hash_ctx)) == NULL) {
        return false;
    }
    if (init) {
        cx_keccak_init(hash_ctx, 256);  // initialize it
    }
    return true;
}

/**
 * Go up (remove) a depth level.
 *
 * @return whether the pop was successful
 */
static bool path_depth_list_pop(void) {
    uint8_t hash[KECCAK256_HASH_BYTESIZE];

    if (path_struct == NULL) {
        return false;
    }
    if (path_struct->depth_count == 0) {
        return false;
    }
    path_struct->depth_count -= 1;

    finalize_hash_depth(hash);
    if (path_struct->depth_count > 0) {
        feed_last_hash_depth(hash);
    } else {
        switch (path_struct->root_type) {
            case ROOT_DOMAIN:
                memcpy(tmpCtx.messageSigningContext712.domainHash, hash, KECCAK256_HASH_BYTESIZE);
                break;
            case ROOT_MESSAGE:
                memcpy(tmpCtx.messageSigningContext712.messageHash, hash, KECCAK256_HASH_BYTESIZE);
                break;
            default:
                break;
        }
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
static bool array_depth_list_push(uint8_t path_idx, uint8_t size) {
    s_array_depth *arr;

    if (path_struct == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }
    if (path_struct->array_depth_count == MAX_ARRAY_DEPTH) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    arr = &path_struct->array_depths[path_struct->array_depth_count];
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
static bool array_depth_list_pop(void) {
    uint8_t hash[KECCAK256_HASH_BYTESIZE];

    if (path_struct == NULL) {
        return false;
    }
    if (path_struct->array_depth_count == 0) {
        return false;
    }

    finalize_hash_depth(hash);
    feed_last_hash_depth(hash);

    path_struct->array_depth_count -= 1;
    return true;
}

/**
 * Updates the path so that it doesn't point to a struct-type field, but rather
 * only to actual fields.
 *
 * @return whether the path update worked or not
 */
static bool path_update(void) {
    uint8_t fields_count;
    const void *struct_ptr;
    const void *field_ptr;
    const char *typename;
    uint8_t typename_len;
    uint8_t hash[KECCAK256_HASH_BYTESIZE];

    if (path_struct == NULL) {
        return false;
    }
    if ((field_ptr = get_field(NULL)) == NULL) {
        return false;
    }
    struct_ptr = path_struct->root_struct;
    while (struct_field_type(field_ptr) == TYPE_CUSTOM) {
        typename = get_struct_field_typename(field_ptr, &typename_len);
        if ((struct_ptr = get_structn(typename, typename_len)) == NULL) {
            return false;
        }
        if ((field_ptr = get_struct_fields_array(struct_ptr, &fields_count)) == NULL) {
            return false;
        }

        if (push_new_hash_depth(true) == false) {
            return false;
        }
        // get the struct typehash
        if (type_hash(typename, typename_len, hash) == false) {
            return false;
        }
        feed_last_hash_depth(hash);

        ui_712_queue_struct_to_review();
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
bool path_set_root(const char *const struct_name, uint8_t name_length) {
    uint8_t hash[KECCAK256_HASH_BYTESIZE];

    if (path_struct == NULL) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }

    path_struct->root_struct = get_structn(struct_name, name_length);

    if (path_struct->root_struct == NULL) {
        PRINTF("Struct name not found (");
        for (int i = 0; i < name_length; ++i) {
            PRINTF("%c", struct_name[i]);
        }
        PRINTF(")!\n");
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }

    if (push_new_hash_depth(true) == false) {
        return false;
    }
    if (type_hash(struct_name, name_length, hash) == false) {
        return false;
    }
    feed_last_hash_depth(hash);
    //

    // init depth, at 0 : empty path
    path_struct->depth_count = 0;
    path_depth_list_push();

    // init array levels at 0
    path_struct->array_depth_count = 0;

    if ((name_length == strlen(DOMAIN_STRUCT_NAME)) &&
        (strncmp(struct_name, DOMAIN_STRUCT_NAME, name_length) == 0)) {
        path_struct->root_type = ROOT_DOMAIN;
    } else {
        path_struct->root_type = ROOT_MESSAGE;
    }

    struct_state = DEFINED;

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
                                      uint8_t size) {
    uint8_t expected_size;
    uint8_t arr_idx;
    e_array_type expected_type;

    arr_idx = (total_count - path_struct->array_depth_count) - 1;
    // we skip index 0, since we already have it
    for (uint8_t idx = 1; idx < (arr_idx + 1); ++idx) {
        if ((depth = get_next_struct_field_array_lvl(depth)) == NULL) {
            return false;
        }
    }
    expected_type = struct_field_array_depth(depth, &expected_size);
    if ((expected_type == ARRAY_FIXED_SIZE) && (expected_size != size)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        PRINTF("Unexpected array depth size. (expected %d, got %d)\n", expected_size, size);
        return false;
    }
    // add it
    if (!array_depth_list_push(pidx, size)) {
        return false;
    }
    return true;
}

/**
 * Add a new array depth with a given size (number of elements).
 *
 * @param[in] data pointer to the number of elements
 * @param[in] length length of data
 * @return whether the add was successful or not
 */
bool path_new_array_depth(const uint8_t *const data, uint8_t length) {
    const void *field_ptr = NULL;
    const void *depth = NULL;
    uint8_t depth_count;
    uint8_t total_count = 0;
    uint8_t pidx;
    bool is_custom;

    if (path_struct == NULL) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    } else if (length != 1) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }

    for (pidx = 0; pidx < path_struct->depth_count; ++pidx) {
        if ((field_ptr = get_nth_field(NULL, pidx + 1)) == NULL) {
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return false;
        }
        if (struct_field_is_array(field_ptr)) {
            if ((depth = get_struct_field_array_lvls_array(field_ptr, &depth_count)) == NULL) {
                apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
                return false;
            }
            total_count += depth_count;
            if (total_count > path_struct->array_depth_count) {
                if (!check_and_add_array_depth(depth, total_count, pidx, *data)) {
                    return false;
                }
                break;
            }
        }
    }

    if (pidx == path_struct->depth_count) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        PRINTF("Did not find a matching array type.\n");
        return false;
    }
    is_custom = struct_field_type(field_ptr) == TYPE_CUSTOM;
    if (push_new_hash_depth(!is_custom) == false) {
        return false;
    }
    if (is_custom) {
        cx_sha3_t *hash_ctx = get_last_hash_ctx();
        cx_sha3_t *old_ctx = hash_ctx - 1;

        memcpy(hash_ctx, old_ctx, sizeof(*old_ctx));
        cx_keccak_init(old_ctx, 256);  // init hash
    }

    return true;
}

/**
 * Advance within the struct that contains the field the path points to.
 *
 * @return whether the end of the struct has been reached.
 */
static bool path_advance_in_struct(void) {
    bool end_reached = true;
    uint8_t *depth = &path_struct->depths[path_struct->depth_count - 1];
    uint8_t fields_count;

    if (path_struct == NULL) {
        return false;
    }
    if ((get_field(&fields_count)) == NULL) {
        return false;
    }
    if (path_struct->depth_count > 0) {
        *depth += 1;
        ui_712_notify_filter_change();
        end_reached = (*depth == fields_count);
    }
    if (end_reached) {
        path_depth_list_pop();
    }
    return end_reached;
}

/**
 * Advance within the array levels of the current field the path points to.
 *
 * @return whether the end of the array levels has been reached.
 */
static bool path_advance_in_array(void) {
    bool end_reached;
    s_array_depth *arr_depth;

    if (path_struct == NULL) {
        return false;
    }
    do {
        end_reached = false;
        arr_depth = &path_struct->array_depths[path_struct->array_depth_count - 1];

        if ((path_struct->array_depth_count > 0) &&
            (arr_depth->path_index == (path_struct->depth_count - 1))) {
            arr_depth->size -= 1;
            if (arr_depth->size == 0) {
                array_depth_list_pop();
                end_reached = true;
            } else {
                return false;
            }
        }
    } while (end_reached);
    return true;
}

/**
 * Updates the path to point to the next field in order (DFS).
 *
 * @return whether the advancement was successful or not
 */
bool path_advance(void) {
    bool end_reached;

    do {
        if (path_advance_in_array()) {
            end_reached = path_advance_in_struct();
        } else {
            end_reached = false;
        }
    } while (end_reached);
    path_update();
    return true;
}

/**
 * Get root structure type from path (domain or message)
 *
 * @return enum representing root type
 */
e_root_type path_get_root_type(void) {
    if (path_struct == NULL) {
        return ROOT_DOMAIN;
    }
    return path_struct->root_type;
}

/**
 * Get root structure from path
 *
 * @return pointer to the root structure definition
 */
const void *path_get_root(void) {
    if (path_struct == NULL) {
        return NULL;
    }
    return path_struct->root_struct;
}

/**
 * Get the current amount of depth
 *
 * @return depth count
 */
uint8_t path_get_depth_count(void) {
    if (path_struct == NULL) {
        return 0;
    }
    return path_struct->depth_count;
}

/**
 * Initialize the path context with its indexes in memory and sets it with a depth of 0.
 *
 * @return whether the memory allocation were successful.
 */
bool path_init(void) {
    if (path_struct == NULL) {
        if ((path_struct = MEM_ALLOC_AND_ALIGN_TYPE(*path_struct)) == NULL) {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        } else {
            path_struct->depth_count = 0;
        }
    }
    return path_struct != NULL;
}

/**
 * De-initialize the path context
 */
void path_deinit(void) {
    path_struct = NULL;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
