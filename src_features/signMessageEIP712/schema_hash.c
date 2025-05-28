#include "schema_hash.h"
#include "hash_bytes.h"
#include "typed_data.h"
#include "format_hash_field_type.h"
#include "context_712.h"

// the SDK does not define a SHA-224 type, define it here so it's easier
// to understand in the code
typedef cx_sha256_t cx_sha224_t;

/**
 * Compute the schema hash
 *
 * The schema hash is the value of the root field "types" in the JSON data,
 * stripped of all its spaces and newlines. This function reconstructs the JSON syntax
 * from the stored typed data.
 *
 * @return whether the schema hash was successful or not
 */
bool compute_schema_hash(void) {
    const s_struct_712 *struct_ptr;
    const s_struct_712_field *field_ptr;
    cx_sha224_t hash_ctx;
    cx_err_t error = CX_INTERNAL_ERROR;

    cx_sha224_init(&hash_ctx);

    struct_ptr = get_struct_list();
    hash_byte('{', (cx_hash_t *) &hash_ctx);
    while (struct_ptr != NULL) {
        hash_byte('"', (cx_hash_t *) &hash_ctx);
        hash_nbytes((uint8_t *) struct_ptr->name,
                    strlen(struct_ptr->name),
                    (cx_hash_t *) &hash_ctx);
        hash_nbytes((uint8_t *) "\":[", 3, (cx_hash_t *) &hash_ctx);
        field_ptr = struct_ptr->fields;
        while (field_ptr != NULL) {
            hash_nbytes((uint8_t *) "{\"name\":\"", 9, (cx_hash_t *) &hash_ctx);
            hash_nbytes((uint8_t *) field_ptr->key_name,
                        strlen(field_ptr->key_name),
                        (cx_hash_t *) &hash_ctx);
            hash_nbytes((uint8_t *) "\",\"type\":\"", 10, (cx_hash_t *) &hash_ctx);
            if (!format_hash_field_type(field_ptr, (cx_hash_t *) &hash_ctx)) {
                return false;
            }
            hash_nbytes((uint8_t *) "\"}", 2, (cx_hash_t *) &hash_ctx);
            if (((s_flist_node *) field_ptr)->next != NULL) {
                hash_byte(',', (cx_hash_t *) &hash_ctx);
            }
            field_ptr = (s_struct_712_field *) ((s_flist_node *) field_ptr)->next;
        }
        hash_byte(']', (cx_hash_t *) &hash_ctx);
        if (((s_flist_node *) struct_ptr)->next != NULL) {
            hash_byte(',', (cx_hash_t *) &hash_ctx);
        }
        struct_ptr = (s_struct_712 *) ((s_flist_node *) struct_ptr)->next;
    }
    hash_byte('}', (cx_hash_t *) &hash_ctx);

    // copy hash into context struct
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &hash_ctx,
                              CX_LAST,
                              NULL,
                              0,
                              eip712_context->schema_hash,
                              sizeof(eip712_context->schema_hash)));
    return true;
end:
    return false;
}
