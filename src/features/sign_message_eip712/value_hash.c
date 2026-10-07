#include "value_hash.h"
#include "typed_data.h"
#include "type_hash.h"
#include "encode_field.h"
#include "app_mem_utils.h"
#include "common_utils.h"    // CX_KECCAK_256_SIZE
#include "shared_context.h"  // tmpCtx

/**
 * Encode a VAL_ATOMIC leaf to 32 bytes.
 * Static types: encode+pad. Dynamic types (string/bytes): keccak256(data).
 */
static bool encode_atomic(const s_struct_712_value *leaf, uint8_t *out) {
    const s_struct_712_field *field = leaf->field;
    const uint8_t *data = leaf->data;
    uint16_t length = leaf->length;
    void *encoded;

    if (IS_DYN(field->type)) {
        cx_sha3_t sha3;
        if (cx_keccak_init_no_throw(&sha3, 256) != CX_OK) {
            PRINTF("encode_atomic: keccak_init failed for dyn field %s\n", field->key_name);
            return false;
        }
        if (cx_hash_update((cx_hash_t *) &sha3, data, length) != CX_OK) {
            return false;
        }
        return cx_hash_final((cx_hash_t *) &sha3, out) == CX_OK;
    }

    switch (field->type) {
        case TYPE_SOL_UINT:
            encoded = encode_uint(data, length);
            break;
        case TYPE_SOL_INT:
            encoded = encode_int(data, length, field->type_size);
            break;
        case TYPE_SOL_BYTES_FIX:
            encoded = encode_bytes(data, length);
            break;
        case TYPE_SOL_ADDRESS:
            encoded = encode_address(data, length);
            break;
        case TYPE_SOL_BOOL:
            encoded = encode_boolean(data, length);
            break;
        default:
            PRINTF("encode_atomic: unknown type %d for field %s\n", field->type, field->key_name);
            return false;
    }
    if (encoded == NULL) {
        PRINTF("encode_atomic: encode failed type=%d len=%d for field %s\n",
               field->type,
               (int) length,
               field->key_name);
        return false;
    }

    memcpy(out, encoded, EIP_712_ENCODED_FIELD_LENGTH);
    APP_MEM_FREE(encoded);
    return true;
}

// One allocation per child, so no single allocation scales with the child count.
typedef struct {
    flist_node_t _list;
    uint8_t hash[CX_KECCAK_256_SIZE];
} s_child_digest;

// to be used as a \ref f_list_node_del
static void delete_child_digest(s_child_digest *digest) {
    APP_MEM_FREE(digest);
}

// Leaves are encoded on the spot, containers were already hashed into @p cursor, in order.
static bool child_digest(const s_struct_712_value *child,
                         const s_child_digest **cursor,
                         uint8_t *out) {
    if (child->kind == VAL_ATOMIC) {
        return encode_atomic(child, out);
    }
    if (*cursor == NULL) {
        return false;
    }
    memcpy(out, (*cursor)->hash, CX_KECCAK_256_SIZE);
    *cursor = (const s_child_digest *) ((const flist_node_t *) *cursor)->next;
    return true;
}

/**
 * Hash an array: keccak256(enc(elem[0]) || enc(elem[1]) || ...).
 * noinline: keeps the cx_sha3_t below out of hash_value()'s recursive frames.
 */
__attribute__((noinline)) static bool hash_array(const s_struct_712_value *arr,
                                                 uint8_t *out,
                                                 const s_child_digest *digests) {
    cx_sha3_t sha3;
    uint8_t elem_hash[CX_KECCAK_256_SIZE];

    if (cx_keccak_init_no_throw(&sha3, 256) != CX_OK) {
        return false;
    }
    for (const s_struct_712_value *elem = arr->children; elem != NULL;
         elem = (const s_struct_712_value *) ((const flist_node_t *) elem)->next) {
        if (!child_digest(elem, &digests, elem_hash)) {
            return false;
        }
        if (cx_hash_update((cx_hash_t *) &sha3, elem_hash, sizeof(elem_hash)) != CX_OK) {
            return false;
        }
    }
    return cx_hash_final((cx_hash_t *) &sha3, out) == CX_OK;
}

/**
 * Hash a struct: keccak256(typeHash || enc(field[0]) || enc(field[1]) || ...).
 * noinline for the same reason as hash_array().
 */
__attribute__((noinline)) static bool hash_struct(const s_struct_712_value *node,
                                                  uint8_t *out,
                                                  const s_child_digest *digests) {
    cx_sha3_t sha3;
    uint8_t type_hash_buf[CX_KECCAK_256_SIZE];
    uint8_t child_hash[CX_KECCAK_256_SIZE];
    const char *name = node->struct_type->name;

    PRINTF("hash_struct: %s\n", name);

    if (!type_hash(name, type_hash_buf)) {
        PRINTF("hash_struct: type_hash failed for %s\n", name);
        return false;
    }

    if (cx_keccak_init_no_throw(&sha3, 256) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &sha3, type_hash_buf, sizeof(type_hash_buf)) != CX_OK) {
        return false;
    }

    for (const s_struct_712_value *child = node->children; child != NULL;
         child = (const s_struct_712_value *) ((const flist_node_t *) child)->next) {
        if (!child_digest(child, &digests, child_hash)) {
            return false;
        }
        if (cx_hash_update((cx_hash_t *) &sha3, child_hash, sizeof(child_hash)) != CX_OK) {
            return false;
        }
    }
    return cx_hash_final((cx_hash_t *) &sha3, out) == CX_OK;
}

static bool hash_value(const s_struct_712_value *node, uint8_t *out, uint8_t depth) {
    s_child_digest *digests = NULL;
    bool ret = true;

    // leaves never recurse, so this bound counts the same nesting levels as the parser's frames
    if ((node->kind == VAL_ATOMIC) || (depth >= TD_MAX_DEPTH)) {
        PRINTF("hash_value: leaf node or max depth exceeded\n");
        return false;
    }

    // pre-hashing the children keeps every hashing context out of the recursion
    for (const s_struct_712_value *child = node->children; (child != NULL) && ret;
         child = (const s_struct_712_value *) ((const flist_node_t *) child)->next) {
        if (child->kind != VAL_ATOMIC) {
            s_child_digest *digest;

            if ((digest = APP_MEM_ALLOC(sizeof(*digest))) == NULL) {
                ret = false;
                break;
            }
            flist_push_back((flist_node_t **) &digests, (flist_node_t *) digest);
            ret = hash_value(child, digest->hash, depth + 1);
        }
    }

    if (ret) {
        ret = (node->kind == VAL_STRUCT) ? hash_struct(node, out, digests)
                                         : hash_array(node, out, digests);
    }
    flist_clear((flist_node_t **) &digests, (f_list_node_del) &delete_child_digest);
    return ret;
}

/**
 * Run hash pass over the complete EIP-712 value tree.
 *
 * Computes hashStruct(domain) and hashStruct(message) and writes results to
 * tmpCtx.messageSigningContext712.domainHash / .messageHash.
 *
 * Domain special fields (chainId, verifyingContract) are extracted when the domain is complete
 * and accessible via td_get_domain_chain_id() / td_get_domain_contract_addr().
 */
bool value_hash_pass(const s_eip712_impl *impl) {
    uint8_t hash[CX_KECCAK_256_SIZE];

    if (impl == NULL) {
        return false;
    }

    if ((impl->domain == NULL) || !hash_value(impl->domain, hash, 0)) {
        PRINTF("value_hash_pass: domain hash failed\n");
        return false;
    }
    memcpy(tmpCtx.messageSigningContext712.domainHash, hash, CX_KECCAK_256_SIZE);

    if ((impl->message == NULL) || !hash_value(impl->message, hash, 0)) {
        PRINTF("value_hash_pass: message hash failed\n");
        return false;
    }
    memcpy(tmpCtx.messageSigningContext712.messageHash, hash, CX_KECCAK_256_SIZE);

    return true;
}
