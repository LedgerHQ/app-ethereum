#include "type_hash.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "format_hash_field_type.h"
#include "typed_data.h"
#include "lists.h"

// Tracks whether the next comma-separated field is the first one
typedef struct {
    cx_sha3_t *hash_ctx;
    bool first;
} s_type_hash_field_ctx;

/**
 * Encode & hash the given structure field
 *
 * @param[in] field_ptr pointer to the struct field
 * @param[in,out] ctx hash context + first-field tracking
 * @return \ref true it finished correctly, \ref false if it didn't (memory allocation)
 */
static bool encode_and_hash_field(const s_struct_712_field *field_ptr, void *context) {
    s_type_hash_field_ctx *ctx = context;
    const char *name;

    if (!ctx->first) {
        if (cx_hash_update((cx_hash_t *) ctx->hash_ctx, (uint8_t *) ",", 1) != CX_OK) {
            return false;
        }
    }
    ctx->first = false;

    if (!format_hash_field_type(field_ptr, (cx_hash_t *) ctx->hash_ctx)) {
        return false;
    }
    // space between field type name and field name
    if (cx_hash_update((cx_hash_t *) ctx->hash_ctx, (uint8_t *) " ", 1) != CX_OK) {
        return false;
    }

    // field name
    name = field_ptr->key_name;
    return cx_hash_update((cx_hash_t *) ctx->hash_ctx, (uint8_t *) name, strlen(name)) == CX_OK;
}

/**
 * Encode & hash the a given structure type
 *
 * @param[in,out] hash_ctx pointer to hash context
 * @param[in] struct_ptr pointer to the structure we want the typestring of
 * @param[in] str_length length of the formatted string in memory
 * @return pointer of the string in memory, \ref NULL in case of an error
 */
static bool encode_and_hash_type(cx_sha3_t *hash_ctx, const s_struct_712 *struct_ptr) {
    const char *struct_name;
    s_type_hash_field_ctx field_ctx = {.hash_ctx = hash_ctx, .first = true};

    // struct name
    struct_name = struct_ptr->name;
    if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) struct_name, strlen(struct_name)) !=
        CX_OK) {
        return false;
    }

    // opening struct parentheses
    if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) "(", 1) != CX_OK) {
        return false;
    }

    if (!td_visit_struct_fields(struct_ptr, encode_and_hash_field, &field_ctx)) {
        return false;
    }
    // closing struct parentheses
    if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) ")", 1) != CX_OK) {
        return false;
    }

    return true;
}

typedef struct struct_dep {
    flist_node_t _list;
    const s_struct_712 *s;
} s_struct_dep;

/**
 * Add a struct to the dependency list if not already present
 *
 * @param[in,out] first_dep pointer to the head of the dependency list
 * @param[in] struct_ptr pointer to the struct to add
 * @return \ref true on success, \ref false on memory allocation failure
 */
static bool add_dep_if_new(s_struct_dep **first_dep, const s_struct_712 *struct_ptr) {
    s_struct_dep *tmp;
    s_struct_dep *new_dep;

    for (tmp = *first_dep; tmp != NULL; tmp = (s_struct_dep *) ((flist_node_t *) tmp)->next) {
        if (tmp->s == struct_ptr) return true;
    }
    if ((new_dep = APP_MEM_ALLOC(sizeof(*new_dep))) == NULL) {
        return false;
    }
    new_dep->s = struct_ptr;
    flist_push_back((flist_node_t **) first_dep, (flist_node_t *) new_dep);
    return true;
}

/**
 * Add a struct dependency for a single field, if it is a TYPE_STRUCT field.
 *
 * @param[in] field_ptr pointer to the struct field being visited
 * @param[in,out] first_dep pointer to the head of the dependency list
 * @return \ref true on success (including "not a struct field", a no-op), \ref false on error
 */
static bool collect_one_dep(const s_struct_712_field *field_ptr, void *context) {
    s_struct_dep **first_dep = context;
    const s_struct_712 *dep;
    const char *dep_name;

    if (field_ptr->type != TYPE_STRUCT) {
        return true;
    }
    dep_name = td_get_struct_field_typename(field_ptr);
    if ((dep = td_find_struct(dep_name)) == NULL) {
        PRINTF("Error: could not find EIP-712 dependency struct \"%s\" during type_hash\n",
               dep_name);
        return false;
    }
    return add_dep_if_new(first_dep, dep);
}

/**
 * Scan all fields of a struct and add any unknown TYPE_STRUCT dependencies
 *
 * @param[in,out] first_dep pointer to the head of the dependency list
 * @param[in] struct_ptr pointer to the struct whose fields are scanned
 * @return \ref true on success, \ref false on error
 */
static bool collect_direct_deps(s_struct_dep **first_dep, const s_struct_712 *struct_ptr) {
    return td_visit_struct_fields(struct_ptr, collect_one_dep, first_dep);
}

/**
 * Find all the transitive dependencies of a given structure
 *
 * Uses the dependency list itself as a worklist: new entries are appended at
 * the back while the cursor advances forward, so newly discovered dependencies
 * are processed without recursion and without extra allocations.
 *
 * @param[out] first_dep pointer to the head of the dependency list
 * @param[in] struct_ptr pointer to the struct we are getting the dependencies of
 * @return \ref true on success, \ref false on error
 */
static bool get_struct_dependencies(s_struct_dep **first_dep, const s_struct_712 *struct_ptr) {
    s_struct_dep *cursor;

    if (!collect_direct_deps(first_dep, struct_ptr)) {
        return false;
    }

    for (cursor = *first_dep; cursor != NULL;
         cursor = (s_struct_dep *) ((flist_node_t *) cursor)->next) {
        if (!collect_direct_deps(first_dep, cursor->s)) {
            return false;
        }
    }
    return true;
}

static bool compare_struct_deps(const s_struct_dep *a, const s_struct_dep *b) {
    const char *name1, *name2;
    size_t namelen1, namelen2;
    int str_cmp_result;

    name1 = a->s->name;
    namelen1 = strlen(name1);
    name2 = b->s->name;
    namelen2 = strlen(name2);

    str_cmp_result = strncmp(name1, name2, MIN(namelen1, namelen2));
    if ((str_cmp_result > 0) || ((str_cmp_result == 0) && (namelen1 > namelen2))) {
        return false;
    }
    return true;
}

// to be used as a \ref f_list_node_del
static void delete_struct_dep(s_struct_dep *sdep) {
    APP_MEM_FREE(sdep);
}

static bool type_hash_internal(const char *struct_name, uint8_t *hash_buf, s_struct_dep **deps) {
    cx_sha3_t hash_ctx;
    const void *struct_ptr;

    if ((struct_ptr = td_find_struct(struct_name)) == NULL) {
        PRINTF("Error: could not find EIP-712 struct \"%s\" for type_hash\n", struct_name);
        return false;
    }
    if (cx_keccak_init_no_throw(&hash_ctx, 256) != CX_OK) {
        return false;
    }
    // get_struct_dependencies may populate `deps` partially before failing;
    // every exit path past this point must release the list.
    if (!get_struct_dependencies(deps, struct_ptr)) {
        return false;
    }
    flist_sort((flist_node_t **) deps, (f_list_node_cmp) &compare_struct_deps);
    if (encode_and_hash_type(&hash_ctx, struct_ptr) == false) {
        return false;
    }
    // loop over each struct and generate string, skipping the struct type itself in
    // case a recursive schema listed it in its own dependency set
    for (const s_struct_dep *tmp = *deps; tmp != NULL;
         tmp = (s_struct_dep *) ((flist_node_t *) tmp)->next) {
        if (tmp->s != struct_ptr) {
            if (encode_and_hash_type(&hash_ctx, tmp->s) == false) {
                return false;
            }
        }
    }

    // copy hash into memory
    if (cx_hash_final((cx_hash_t *) &hash_ctx, hash_buf) != CX_OK) {
        return false;
    }
    return true;
}

/**
 * Encode the structure's type and hash it
 *
 * @param[in] struct_name name of the given struct
 * @param[out] hash_buf buffer containing the resulting type_hash
 * @return whether the type_hash was successful or not
 */
bool type_hash(const char *struct_name, uint8_t *hash_buf) {
    s_struct_dep *deps = NULL;
    bool ret;

    ret = type_hash_internal(struct_name, hash_buf, &deps);
    flist_clear((flist_node_t **) &deps, (f_list_node_del) &delete_struct_dep);
    return ret;
}
