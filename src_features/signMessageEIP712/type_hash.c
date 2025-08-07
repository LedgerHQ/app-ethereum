#include "mem.h"
#include "mem_utils.h"
#include "type_hash.h"
#include "format_hash_field_type.h"
#include "hash_bytes.h"
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"
#include "list.h"

/**
 * Encode & hash the given structure field
 *
 * @param[in] field_ptr pointer to the struct field
 * @return \ref true it finished correctly, \ref false if it didn't (memory allocation)
 */
static bool encode_and_hash_field(const s_struct_712_field *field_ptr) {
    const char *name;

    if (!format_hash_field_type(field_ptr, (cx_hash_t *) &global_sha3)) {
        return false;
    }
    // space between field type name and field name
    hash_byte(' ', (cx_hash_t *) &global_sha3);

    // field name
    name = field_ptr->key_name;
    hash_nbytes((uint8_t *) name, strlen(name), (cx_hash_t *) &global_sha3);
    return true;
}

/**
 * Encode & hash the a given structure type
 *
 * @param[in] struct_ptr pointer to the structure we want the typestring of
 * @param[in] str_length length of the formatted string in memory
 * @return pointer of the string in memory, \ref NULL in case of an error
 */
static bool encode_and_hash_type(const s_struct_712 *struct_ptr) {
    const char *struct_name;
    const s_struct_712_field *field_ptr;

    // struct name
    struct_name = struct_ptr->name;
    hash_nbytes((uint8_t *) struct_name, strlen(struct_name), (cx_hash_t *) &global_sha3);

    // opening struct parentheses
    hash_byte('(', (cx_hash_t *) &global_sha3);

    for (field_ptr = struct_ptr->fields; field_ptr != NULL;
         field_ptr = (s_struct_712_field *) ((s_flist_node *) field_ptr)->next) {
        // comma separating struct fields
        if (field_ptr != struct_ptr->fields) {
            hash_byte(',', (cx_hash_t *) &global_sha3);
        }

        if (encode_and_hash_field(field_ptr) == false) {
            return NULL;
        }
    }
    // closing struct parentheses
    hash_byte(')', (cx_hash_t *) &global_sha3);

    return true;
}

typedef struct struct_dep {
    s_flist_node _list;
    const s_struct_712 *s;
} s_struct_dep;

/**
 * Find all the dependencies from a given structure
 *
 * @param[out] deps_count count of how many struct dependency pointers
 * @param[in] first_dep pointer to the first dependency pointer
 * @param[in] struct_ptr pointer to the struct we are getting the dependencies of
 * @return pointer to the first found dependency, \ref NULL otherwise
 */
static bool get_struct_dependencies(s_struct_dep **first_dep, const s_struct_712 *struct_ptr) {
    const s_struct_712_field *field_ptr;
    const char *arg_structname;
    const s_struct_712 *arg_struct_ptr;
    s_struct_dep *tmp;
    s_struct_dep *new_dep;

    for (field_ptr = struct_ptr->fields; field_ptr != NULL;
         field_ptr = (s_struct_712_field *) ((s_flist_node *) field_ptr)->next) {
        if (field_ptr->type == TYPE_CUSTOM) {
            // get struct name
            arg_structname = get_struct_field_typename(field_ptr);
            // from its name, get the pointer to its definition
            if ((arg_struct_ptr = get_structn(arg_structname, strlen(arg_structname))) == NULL) {
                PRINTF("Error: could not find EIP-712 dependency struct \"");
                for (int i = 0; i < (int) strlen(arg_structname); ++i)
                    PRINTF("%c", arg_structname[i]);
                PRINTF("\" during type_hash\n");
                return false;
            }

            // check if it is not already present in the dependencies array
            for (tmp = *first_dep; tmp != NULL;
                 tmp = (s_struct_dep *) ((s_flist_node *) tmp)->next) {
                // it's a match!
                if (tmp->s == arg_struct_ptr) {
                    break;
                }
            }
            // if it's not present in the array, add it and recurse into it
            if (tmp == NULL) {
                if ((new_dep = app_mem_alloc(sizeof(s_struct_dep))) == NULL) {
                    apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
                    return false;
                }
                explicit_bzero(new_dep, sizeof(*new_dep));
                new_dep->s = arg_struct_ptr;
                flist_push_back((s_flist_node **) first_dep, (s_flist_node *) new_dep);
                // TODO: Move away from recursive calls
                get_struct_dependencies(first_dep, arg_struct_ptr);
            }
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
    app_mem_free(sdep);
}

/**
 * Encode the structure's type and hash it
 *
 * @param[in] struct_name name of the given struct
 * @param[in] struct_name_length length of the name of the given struct
 * @param[out] hash_buf buffer containing the resulting type_hash
 * @return whether the type_hash was successful or not
 */
bool type_hash(const char *struct_name, const uint8_t struct_name_length, uint8_t *hash_buf) {
    const void *struct_ptr;
    s_struct_dep *deps;
    cx_err_t error = CX_INTERNAL_ERROR;

    if ((struct_ptr = get_structn(struct_name, struct_name_length)) == NULL) {
        PRINTF("Error: could not find EIP-712 struct \"");
        for (int i = 0; i < struct_name_length; ++i) PRINTF("%c", struct_name[i]);
        PRINTF("\" for type_hash\n");
        return false;
    }
    CX_CHECK(cx_keccak_init_no_throw(&global_sha3, 256));
    deps = NULL;
    if (!get_struct_dependencies(&deps, struct_ptr)) {
        return false;
    }
    flist_sort((s_flist_node **) &deps, (f_list_node_cmp) &compare_struct_deps);
    if (encode_and_hash_type(struct_ptr) == false) {
        return false;
    }
    // loop over each struct and generate string
    for (const s_struct_dep *tmp = deps; tmp != NULL;
         tmp = (s_struct_dep *) ((s_flist_node *) tmp)->next) {
        if (encode_and_hash_type(tmp->s) == false) {
            return false;
        }
    }

    flist_clear((s_flist_node **) &deps, (f_list_node_del) &delete_struct_dep);
    // copy hash into memory
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              CX_LAST,
                              NULL,
                              0,
                              hash_buf,
                              KECCAK256_HASH_BYTESIZE));
    return true;
end:
    return false;
}
