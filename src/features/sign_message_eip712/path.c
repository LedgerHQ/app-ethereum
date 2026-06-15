#include <string.h>
#include "path.h"
#include "app_mem_utils.h"
#include "context_712.h"
#include "commands_712.h"
#include "type_hash.h"
#include "mem_utils.h"
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"
#include "hash_bytes.h"

static s_path *path_struct = NULL;
static s_path *path_backup = NULL;

static s_hash_ctx *g_hash_ctxs = NULL;

/**
 * Get the field pointer to by the first N depths of the given path
 *
 * @param[in] path given path struct
 * @param[out] fields_count_ptr the number of fields in the last evaluated depth
 * @param[in] n the number of depths to evaluate
 * @return the field which the first Nth depths points to
 */
static const void *get_nth_field_from(const s_path *path, uint8_t *fields_count_ptr, uint8_t n) {
    const s_struct_712 *struct_ptr = NULL;
    const s_struct_712_field *field_ptr = NULL;
    const char *typename;

    if (path == NULL) {
        return NULL;
    }

    struct_ptr = path->root_struct;

    if (n > path->depth_count)  // sanity check
    {
        return NULL;
    }
    for (uint8_t depth = 0; depth < n; ++depth) {
        if ((field_ptr = struct_ptr->fields) == NULL) {
            return NULL;
        }
        if (fields_count_ptr != NULL) {
            *fields_count_ptr = 0;
            for (const s_struct_712_field *tmp = field_ptr; tmp != NULL;
                 tmp = (s_struct_712_field *) ((flist_node_t *) tmp)->next) {
                *fields_count_ptr += 1;
            }
        }

        for (uint8_t index = 0; index < path->depths[depth]; ++index) {
            if ((field_ptr = (s_struct_712_field *) ((flist_node_t *) field_ptr)->next) == NULL) {
                return NULL;
            }
        }
        if (field_ptr->type == TYPE_CUSTOM) {
            typename = get_struct_field_typename(field_ptr);
            if ((struct_ptr = get_structn(typename, strlen(typename))) == NULL) {
                return NULL;
            }
        }
    }
    return field_ptr;
}

static const void *get_nth_field(uint8_t *fields_count_ptr, uint8_t n) {
    return get_nth_field_from(path_struct, fields_count_ptr, n);
}

/**
 * Get the element the path is pointing to.
 *
 * @param[out] the number of fields in the depth of the returned field
 * @return the field which the path points to
 */
static inline const void *get_field(uint8_t *fields_count) {
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

const void *path_backup_get_nth_field(uint8_t n) {
    return get_nth_field_from(path_backup, NULL, n);
}

/**
 * Get Nth to last struct field from path
 *
 * @param[in] n nth to last depth requested
 * @return pointer to the matching field, \ref NULL otherwise
 */
const void *path_get_nth_field_to_last(uint8_t n) {
    const char *typename;
    const void *field_ptr;
    const void *struct_ptr = NULL;

    field_ptr = get_nth_field(NULL, path_struct->depth_count - n);
    if (field_ptr != NULL) {
        typename = get_struct_field_typename(field_ptr);
        struct_ptr = get_structn(typename, strlen(typename));
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
 * @return whether the push was successful
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
s_hash_ctx *get_last_hash_ctx(void) {
    flist_node_t *hash_ctx = (flist_node_t *) g_hash_ctxs;

    while ((hash_ctx != NULL) && (hash_ctx->next != NULL)) {
        hash_ctx = hash_ctx->next;
    }
    return (s_hash_ctx *) hash_ctx;
}

/**
 * Get the previous hashing context before the given one
 *
 * @return pointer to the hashing context
 */
static s_hash_ctx *get_previous_hash_ctx(s_hash_ctx *hash_ctx) {
    if (hash_ctx == NULL) {
        return NULL;
    }
    return (s_hash_ctx *) ((list_node_t *) hash_ctx)->prev;
}

/**
 * Hashing context of the struct open at a given path depth.
 *
 * The top context is the deepest open struct, and array accumulators are always
 * spliced below their element struct, so only descendant structs lie above a
 * given depth -- the wanted context is (depth_count - 1 - target_depth) down.
 *
 * @param[in] target_depth 0-based path depth of the struct
 * @return the matching context, or NULL on inconsistency
 */
static s_hash_ctx *get_struct_ctx_at_depth(uint8_t target_depth) {
    s_hash_ctx *ctx = get_last_hash_ctx();
    uint8_t up;

    if (path_struct == NULL) {
        return NULL;
    }
    if ((target_depth + 1) > path_struct->depth_count) {
        return NULL;
    }
    up = (path_struct->depth_count - 1) - target_depth;
    for (uint8_t i = 0; (i < up) && (ctx != NULL); ++i) {
        ctx = get_previous_hash_ctx(ctx);
    }
    return ctx;
}

/**
 * Hashing context of the element struct of an array field at path index pidx
 * (open at depth pidx + 1). Anchoring the array-fold splice here -- not on
 * successor(start)/last -- is what makes nested and struct-first elements fold
 * at the correct shared element context.
 *
 * @param[in] pidx path index of the array field
 * @return the element struct context, or NULL on inconsistency
 */
static s_hash_ctx *get_element_struct_ctx(uint8_t pidx) {
    if ((pidx + 1) > path_struct->depth_count) {
        return NULL;
    }
    return get_struct_ctx_at_depth(pidx + 1);
}

// to be used as a \ref f_list_node_del
static void delete_hash_ctx(s_hash_ctx *ctx) {
    APP_MEM_FREE(ctx);
}

static void remove_last_hash_ctx(void) {
    list_pop_back((list_node_t **) &g_hash_ctxs, (f_list_node_del) &delete_hash_ctx);
}

/**
 * Finalize the last hashing context
 *
 * @param[out] hash pointer to buffer where the hash will be stored
 * @return whether there was anything hashed at this depth
 */
static bool finalize_hash_depth(uint8_t *hash) {
    const s_hash_ctx *hash_ctx;
    bool any_absorbed;

    if ((hash_ctx = get_last_hash_ctx()) == NULL) {
        return false;
    }
    // `blen` is only the pending partial-block length, so it wraps to 0 when the
    // absorbed length is a multiple of the 136-byte keccak rate (e.g. a 16-field
    // struct = 544 = 4*136). Also check the processed-block counter, else such a
    // struct looks empty and is dropped from the array fold (CWE-682).
    any_absorbed = (hash_ctx->hash.blen > 0) || (hash_ctx->hash.header.counter > 0);
    // finalize hash
    if (finalize_hash((cx_hash_t *) &hash_ctx->hash, hash, KECCAK256_HASH_BYTESIZE) != true) {
        return false;
    }
    remove_last_hash_ctx();
    return any_absorbed;
}

/**
 * Continue last progressive hashing context with given hash
 *
 * @param[in] hash pointer to given hash
 */
static bool feed_last_hash_depth(const uint8_t *hash) {
    const s_hash_ctx *hash_ctx;

    if ((hash_ctx = get_last_hash_ctx()) == NULL) {
        return false;
    }
    // continue progressive hash with the array hash
    if (cx_hash_no_throw((cx_hash_t *) &hash_ctx->hash,
                         0,
                         hash,
                         KECCAK256_HASH_BYTESIZE,
                         NULL,
                         0) != CX_OK) {
        return false;
    }
    return true;
}

/**
 * Create a new hashing context depth in memory
 *
 * @param[in] init if the hashing context should be initialized
 * @return whether the memory allocation of the hashing context was successful
 */
static bool push_new_hash_depth(bool init) {
    s_hash_ctx *hash_ctx;

    // allocate new hash context
    if (APP_MEM_CALLOC((void **) &hash_ctx, sizeof(*hash_ctx)) == false) {
        return false;
    }
    if (init) {
        if (cx_keccak_init_no_throw(&hash_ctx->hash, 256) != CX_OK) {
            APP_MEM_FREE(hash_ctx);
            return false;
        }
    }

    list_push_back((list_node_t **) &g_hash_ctxs, (list_node_t *) hash_ctx);
    return true;
}

/**
 * Splice a fresh empty array-accumulator context immediately before `ref` (the
 * element struct context); each finalized element hash folds into it. No keccak
 * state is copied, so already-absorbed bytes are preserved -- unlike the previous
 * memcpy/re-init juggle that corrupted nested arrays.
 *
 * @param[in] ref context to insert before (the element accumulator)
 * @return whether allocation + insertion succeeded
 */
static bool insert_array_hash_depth_before(s_hash_ctx *ref) {
    s_hash_ctx *arr_ctx;

    if (ref == NULL) {
        return false;
    }
    if (APP_MEM_CALLOC((void **) &arr_ctx, sizeof(*arr_ctx)) == false) {
        return false;
    }
    if (cx_keccak_init_no_throw(&arr_ctx->hash, 256) != CX_OK) {
        APP_MEM_FREE(arr_ctx);
        return false;
    }
    if (list_insert_before((list_node_t **) &g_hash_ctxs,
                           (list_node_t *) ref,
                           (list_node_t *) arr_ctx) == false) {
        APP_MEM_FREE(arr_ctx);
        return false;
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
    bool to_feed;

    if (path_struct == NULL) {
        return false;
    }
    if (path_struct->depth_count == 0) {
        return false;
    }
    path_struct->depth_count -= 1;

    to_feed = finalize_hash_depth(hash);
    if (path_struct->depth_count > 0) {
        if (to_feed) {
            if (feed_last_hash_depth(hash) == false) {
                return false;
            }
        }
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
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }
    if (path_struct->array_depth_count == MAX_ARRAY_DEPTH) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    arr = &path_struct->array_depths[path_struct->array_depth_count];
    arr->path_index = path_idx;
    arr->size = size;
    arr->index = 0;
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

    finalize_hash_depth(hash);  // return value not checked on purpose
    if (feed_last_hash_depth(hash) == false) {
        return false;
    }

    path_struct->array_depth_count -= 1;
    return true;
}

/**
 * Updates the path so that it doesn't point to a struct-type field, but rather
 * only to actual fields.
 *
 * @param[in] skip_if_array skip if path is already pointing at an array
 * @param[in] stop_at_array stop at the first downstream array
 * @param[in] do_typehash if a typehash needs to be done when a new struct is encountered
 * @return whether the path update worked or not
 */
static bool path_update(bool skip_if_array, bool stop_at_array, bool do_typehash) {
    const s_struct_712 *struct_ptr;
    const s_struct_712_field *starting_field_ptr;
    const s_struct_712_field *field_ptr;
    const s_struct_712_field *outer_field;
    const char *typename;
    uint8_t hash[KECCAK256_HASH_BYTESIZE];

    if (path_struct == NULL) {
        return false;
    }
    if ((starting_field_ptr = get_field(NULL)) == NULL) {
        return false;
    }
    field_ptr = starting_field_ptr;
    while (field_ptr->type == TYPE_CUSTOM) {
        // check if we meet one of the given conditions
        if (((field_ptr == starting_field_ptr) && skip_if_array) ||
            ((field_ptr != starting_field_ptr) && stop_at_array)) {
            if (field_ptr->type_is_array) {
                // Stop descent unless this field is the currently-iterated outer array.
                // In that case we must descend to set up the new struct hash context.
                // For any nested inner array we stop here and let path_new_array_depth
                // handle its own setup, so that it is captured at the right stack level.
                bool is_outer_array = false;
                if (path_struct->array_depth_count > 0) {
                    outer_field = get_nth_field(
                        NULL,
                        path_struct->array_depths[path_struct->array_depth_count - 1].path_index +
                            1);
                    is_outer_array = (outer_field != NULL) && (outer_field == field_ptr);
                }
                if (!is_outer_array) {
                    break;
                }
            }
        }
        typename = get_struct_field_typename(field_ptr);
        if ((struct_ptr = get_structn(typename, strlen(typename))) == NULL) {
            return false;
        }
        if ((field_ptr = struct_ptr->fields) == NULL) {
            return false;
        }

        if (push_new_hash_depth(true) == false) {
            return false;
        }

        if (do_typehash) {
            // get the struct typehash
            if (type_hash(typename, strlen(typename), hash) == false) {
                return false;
            }
            if (feed_last_hash_depth(hash) == false) {
                return false;
            }
        }

        // TODO: Find a better way to show inner structs in verbose mode when it might be
        //       an empty array of structs in which case we don't want to show it but the
        //       size is only known later
        // ui_712_queue_struct_to_review();
        // Without this check, a recursive or cyclic custom-type graph (A { A a }
        // or A -> B -> A) would keep this loop pushing hash depths past
        // MAX_PATH_DEPTH while ignoring the failure, allocating one new keccak
        // context per iteration until heap exhaustion (CWE-400).
        if (!path_depth_list_push()) {
            apdu_response_code = SWO_INCORRECT_DATA;
            return false;
        }
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
bool path_set_root(const char *struct_name, uint8_t name_length) {
    const s_struct_712 *new_root;
    uint8_t hash[KECCAK256_HASH_BYTESIZE];

    if (path_struct == NULL) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    if ((new_root = get_structn(struct_name, name_length)) == NULL) {
        return false;
    }
    if (new_root == path_struct->root_struct) {
        PRINTF("Error: already at that root struct!\n");
        return false;
    }
    path_struct->root_struct = new_root;

    if (path_struct->root_struct == NULL) {
        PRINTF("Error: struct name not found (");
        for (int i = 0; i < name_length; ++i) {
            PRINTF("%c", struct_name[i]);
        }
        PRINTF(")!\n");
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    if (push_new_hash_depth(true) == false) {
        return false;
    }
    if (type_hash(struct_name, name_length, hash) == false) {
        return false;
    }
    if (feed_last_hash_depth(hash) == false) {
        return false;
    }

    // init depth, at 0 : empty path
    path_struct->depth_count = 0;
    path_depth_list_push();

    // init array levels at 0
    path_struct->array_depth_count = 0;

    if ((name_length == strlen(DOMAIN_STRUCT_NAME)) &&
        (strncmp(struct_name, DOMAIN_STRUCT_NAME, name_length) == 0)) {
        if (path_struct->root_type != ROOT_NONE) {
            return false;
        }
        path_struct->root_type = ROOT_DOMAIN;
    } else {
        if (path_struct->root_type != ROOT_DOMAIN) {
            return false;
        }
        path_struct->root_type = ROOT_MESSAGE;
    }

    struct_state = DEFINED;

    // because the first field could be a struct type
    path_update(true, true, true);
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
static bool check_and_add_array_depth(s_struct_712_field_array_level *array_lvl,
                                      uint8_t total_count,
                                      uint8_t pidx,
                                      uint8_t size) {
    uint8_t arr_idx;

    arr_idx = (total_count - path_struct->array_depth_count) - 1;
    array_lvl += arr_idx;
    if ((array_lvl->type == ARRAY_FIXED_SIZE) && (array_lvl->size != size)) {
        apdu_response_code = SWO_INCORRECT_DATA;
        PRINTF("Unexpected array depth size. (expected %d, got %d)\n", array_lvl->size, size);
        return false;
    }
    // add it
    if (!array_depth_list_push(pidx, size)) {
        return false;
    }
    return true;
}

/**
 * Back-up the current path
 *
 * Used for the handling of discarded filtered fields
 */
static void backup_path(void) {
    const s_struct_712_field *field_ptr;

    memcpy(path_backup, path_struct, sizeof(*path_backup));
    // decrease while it does not point to an array type
    while (path_backup->depth_count > 1) {
        if ((field_ptr = path_backup_get_nth_field(path_backup->depth_count)) == NULL) {
            return;
        }
        if (field_ptr->type_is_array) {
            break;
        }
        path_backup->depth_count -= 1;
    }
}

/**
 * Add a new array depth with a given size (number of elements).
 *
 * @param[in] data pointer to the number of elements
 * @param[in] length length of data
 * @return whether the add was successful or not
 */
bool path_new_array_depth(const uint8_t *data, uint8_t length) {
    const s_struct_712_field *field_ptr = NULL;
    uint8_t total_count = 0;
    uint8_t pidx;
    bool is_custom;
    uint8_t array_size;
    uint8_t array_depth_count_bak;
    s_hash_ctx *start_hash_ctx = get_last_hash_ctx();

    if (path_struct == NULL) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    } else if (length != 1) {
        apdu_response_code = SWO_INCORRECT_DATA;
        return false;
    }

    array_size = *data;
    // Record the path index of the immediately-enclosing OPEN array depth (if
    // any) BEFORE we add this one. If, after we resolve which field this new
    // array level belongs to, that field's path index matches, then this is an
    // INNER dimension of a multi-dimensional array of the SAME field (e.g. the
    // second [..] of T[..][..]) -- which needs the nested-array fold handling
    // below, distinct from a fresh single-level array.
    bool had_enclosing_array = (path_struct->array_depth_count > 0);
    uint8_t enclosing_pidx =
        had_enclosing_array
            ? path_struct->array_depths[path_struct->array_depth_count - 1].path_index
            : 0xff;
    if (array_size == 0) {
        backup_path();
    }
    if (!path_update(false, array_size > 0, array_size > 0)) {
        return false;
    }
    array_depth_count_bak = path_struct->array_depth_count;
    for (pidx = 0; pidx < path_struct->depth_count; ++pidx) {
        if ((field_ptr = get_nth_field(NULL, pidx + 1)) == NULL) {
            apdu_response_code = SWO_INCORRECT_DATA;
            return false;
        }
        if (field_ptr->type_is_array) {
            if (field_ptr->array_levels == NULL) {
                apdu_response_code = SWO_INCORRECT_DATA;
                return false;
            }
            total_count += field_ptr->array_level_count;
            if (total_count > path_struct->array_depth_count) {
                if (!check_and_add_array_depth(field_ptr->array_levels,
                                               total_count,
                                               pidx,
                                               array_size)) {
                    return false;
                }
                break;
            }
        }
    }

    if (pidx == path_struct->depth_count) {
        apdu_response_code = SWO_INCORRECT_DATA;
        PRINTF("Did not find a matching array type.\n");
        return false;
    }
    is_custom = field_ptr->type == TYPE_CUSTOM;
    if (is_custom && (array_size > 0)) {
        // Non-empty custom array: splice a dedicated accumulator below the element
        // struct context (no keccak state copied). Anchor on the element struct by
        // path depth (pidx + 1): successor(start) or last would mis-place it for
        // inner dimensions (GAP-A) or struct-first elements (Class R). All
        // dimensions of one field splice below the shared element context, giving
        // [.., arr_outer, arr_inner, E].
        if (start_hash_ctx == NULL) {
            return false;
        }
        s_hash_ctx *element_ctx = get_element_struct_ctx(pidx);
        if (element_ctx == NULL) {
            return false;
        }
        if (insert_array_hash_depth_before(element_ctx) == false) {
            return false;
        }
    } else if (!is_custom) {
        // Scalar array: push a fresh accumulator (an empty one -> keccak256("")).
        if (push_new_hash_depth(true) == false) {
            return false;
        }
    } else if (is_custom) {
        // Empty custom array -> keccak256(""). Re-init the fresh top context and
        // every struct context path_update descended into for this element, so no
        // typeHash leaks into the fold. Compute the clear boundary now, while the
        // stack still maps 1:1 to path depths (before pushing the extra context).
        if (start_hash_ctx == NULL) {
            return false;
        }
        bool nested_inner_dim = had_enclosing_array && (enclosing_pidx == pidx);
        s_hash_ctx *stop_ctx;
        if (nested_inner_dim) {
            // Inner dimension of a nested array (GAP-B: Leaf[][] = [[]]): clear
            // through the element struct so keccak256("") folds into the outer
            // accumulator (-> keccak256(keccak256(""))).
            s_hash_ctx *element_ctx = get_element_struct_ctx(pidx);
            stop_ctx = (element_ctx != NULL) ? get_previous_hash_ctx(element_ctx) : start_hash_ctx;
        } else {
            // Empty array that is a field of some struct: fold keccak256("") into
            // the containing struct (open at depth pidx). Clearing down to it
            // (exclusive) also drops any spurious element-struct context left by an
            // enclosing nested array. For a single-level array this is start_hash_ctx.
            stop_ctx = get_struct_ctx_at_depth(pidx);
            if (stop_ctx == NULL) {
                stop_ctx = start_hash_ctx;
            }
        }
        if (push_new_hash_depth(false) == false) {
            return false;
        }
        s_hash_ctx *hash_ctx = get_last_hash_ctx();
        while ((hash_ctx != NULL) && (hash_ctx != stop_ctx)) {
            if (cx_keccak_init_no_throw((cx_sha3_t *) &hash_ctx->hash, 256) != CX_OK) {
                return false;
            }
            hash_ctx = get_previous_hash_ctx(hash_ctx);
        }
    }
    if (array_size == 0) {
        // Unwind the empty array level(s). For an empty SCALAR array pass
        // do_typehash=true so a following custom-struct field still gets its
        // typeHash (Class C: {uint256[] a = [], S1 b}). For an empty CUSTOM array
        // keep it false (path_update already descended into the element); a
        // following struct's typeHash is repaired below.
        const bool unwind_typehash = !is_custom;
        do {
            path_advance(unwind_typehash);
        } while (path_struct->array_depth_count > array_depth_count_bak);

        if (is_custom) {
            // The do_typehash=false unwind may have descended into a following or
            // sibling custom struct without feeding its typeHash (Class C custom:
            // Batch{Call[] = [], Meta}; or S0{S2 a, S2 b}; S2{S3[] x}; a.x=b.x=[]).
            // Walk depths top-down; while the opening field is a custom struct whose
            // accumulator is still empty (blen == 0 && counter == 0), feed its
            // typeHash. Stop at the first populated accumulator.
            s_hash_ctx *ctx = get_last_hash_ctx();
            for (uint8_t d = path_struct->depth_count; (d >= 2) && (ctx != NULL); --d) {
                if ((ctx->hash.blen != 0) || (ctx->hash.header.counter != 0)) {
                    break;
                }
                const s_struct_712_field *opener = get_nth_field(NULL, d - 1);
                if ((opener == NULL) || (opener->type != TYPE_CUSTOM)) {
                    break;
                }
                const char *typename = get_struct_field_typename(opener);
                uint8_t hash[KECCAK256_HASH_BYTESIZE];
                if (type_hash(typename, strlen(typename), hash) == false) {
                    return false;
                }
                if (cx_hash_no_throw((cx_hash_t *) &ctx->hash,
                                     0,
                                     hash,
                                     KECCAK256_HASH_BYTESIZE,
                                     NULL,
                                     0) != CX_OK) {
                    return false;
                }
                ctx = get_previous_hash_ctx(ctx);
            }
        }
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
    uint8_t fields_count;

    if (path_struct == NULL) {
        return false;
    }
    if ((get_field(&fields_count)) == NULL) {
        return false;
    }
    if (path_struct->depth_count > 0) {
        uint8_t *depth = &path_struct->depths[path_struct->depth_count - 1];
        *depth += 1;
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
            arr_depth->index += 1;
            if (arr_depth->index == arr_depth->size) {
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
bool path_advance(bool do_typehash) {
    bool end_reached;

    do {
        if (path_advance_in_array()) {
            end_reached = path_advance_in_struct();
        } else {
            end_reached = false;
        }
    } while (end_reached);
    return path_update(true, true, do_typehash);
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
const s_struct_712 *path_get_root(void) {
    if (path_struct == NULL) {
        return NULL;
    }
    return path_struct->root_struct;
}

/**
 * Get the current amount of depth in a given path struct
 *
 * @param[in] given path struct
 * @return depth count
 */
static uint8_t get_depth_count(const s_path *path) {
    if (path == NULL) {
        return 0;
    }
    return path->depth_count;
}

/**
 * Get the current amount of depth in the path
 *
 * @return depth count
 */
uint8_t path_get_depth_count(void) {
    return get_depth_count(path_struct);
}

/**
 * Get the current amount of depth in the backup path
 *
 * @return depth count
 */
uint8_t path_backup_get_depth_count(void) {
    return get_depth_count(path_backup);
}

/**
 * Check if the given relative path exists in the backup path
 *
 * @param[in] path given path
 * @param[in] length length of the path
 * @return whether it exists or not
 */
bool path_exists_in_backup(const char *path, size_t length) {
    size_t offset = 0;
    size_t i;
    const s_struct_712_field *field_ptr;
    const char *typename;
    const s_struct_712 *struct_ptr;
    const char *key;

    if ((field_ptr = get_nth_field_from(path_backup, NULL, path_backup->depth_count)) == NULL) {
        return false;
    }
    while (offset < length) {
        if (((offset + 1) > length) || (memcmp(path + offset, ".", 1) != 0)) {
            return false;
        }
        offset += 1;
        if (((offset + 2) <= length) && (memcmp(path + offset, "[]", 2) == 0)) {
            if (!field_ptr->type_is_array) {
                return false;
            }
            offset += 2;
        } else if (offset < length) {
            for (i = 0; ((offset + i) < length) && (path[offset + i] != '.'); ++i);
            typename = field_ptr->type_name;
            if ((struct_ptr = get_structn(typename, strlen(typename))) == NULL) {
                return false;
            }
            for (field_ptr = struct_ptr->fields; field_ptr != NULL;
                 field_ptr = (s_struct_712_field *) ((flist_node_t *) field_ptr)->next) {
                key = field_ptr->key_name;
                if ((strlen(key) == i) && (memcmp(key, path + offset, i) == 0)) {
                    break;
                }
            }
            if (field_ptr == NULL) {
                return false;
            }
            offset += i;
        } else {
            return false;
        }
    }
    return true;
}

/**
 * Initialize the path context with its indexes in memory and sets it with a depth of 0.
 *
 * @return whether the memory allocation were successful.
 */
bool path_init(void) {
    if (path_struct != NULL) {
        path_deinit();
        return false;
    }

    if ((APP_MEM_CALLOC((void **) &path_struct, sizeof(*path_struct)) == false) ||
        (APP_MEM_CALLOC((void **) &path_backup, sizeof(*path_backup)) == false)) {
        apdu_response_code = SWO_INSUFFICIENT_MEMORY;
    }
    return (path_struct != NULL) && (path_backup != NULL);
}

/**
 * De-initialize the path context
 */
void path_deinit(void) {
    APP_MEM_FREE_AND_NULL((void **) &path_struct);
    APP_MEM_FREE_AND_NULL((void **) &path_backup);
    list_clear((list_node_t **) &g_hash_ctxs, (f_list_node_del) &delete_hash_ctx);
}
