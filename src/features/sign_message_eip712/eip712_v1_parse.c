#include "app_mem_utils.h"
#include "read.h"
#include "os_utils.h"  // is_zeroes_buffer
#include "eip712_v1_parse.h"
#include "typed_data.h"

static s_struct_712 *g_pending_struct = NULL;

// domain root node while its value tree is being built (NULL once popped or before v1_set_root)
static const s_struct_712_value *g_domain_root = NULL;

/**
 * Validate a schema name (struct, type or field name).
 *
 * Names are embedded verbatim into the JSON-like schema hash preimage, so
 * characters that can alter the JSON structure or its parsing would let a host
 * craft distinct schemas whose reconstructed JSON collides. EIP-712 itself does
 * not constrain the charset; this is a Ledger-side hardening choice.
 */
static bool is_valid_identifier(const uint8_t *name, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        // '"' and '\' break the schema-hash JSON reconstruction; '.' and NUL
        // break the dot-joined filter path identity
        if ((name[i] == '"') || (name[i] == '\\') || (name[i] == '.') || (name[i] == '\0')) {
            return false;
        }
    }
    return true;
}

/**
 * Set struct name
 *
 * @param[in] length name length
 * @param[in] name name
 * @return whether it was successful
 */
bool v1_set_struct_name(uint8_t length, const uint8_t *name) {
    if ((name == NULL) || !is_valid_identifier(name, length)) {
        return false;
    }

    char *name_tmp = APP_MEM_ALLOC(length + 1);
    if (name_tmp == NULL) {
        return false;
    }
    memcpy(name_tmp, name, length);
    name_tmp[length] = '\0';

    g_pending_struct = td_create_struct_def(name_tmp);
    APP_MEM_FREE_AND_NULL((void **) &name_tmp);

    if (g_pending_struct == NULL) {
        return false;
    }
    return td_add_struct_def(g_pending_struct);
}

// TypeDesc masks
#define TYPE_MASK     (0xF)
#define ARRAY_MASK    (1 << 7)
#define TYPESIZE_MASK (1 << 6)
#define TYPENAME_ENUM (0xF)

/**
 * Set struct field TypeDesc
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful or not
 */
static bool v1_set_struct_field_typedesc(const uint8_t *data,
                                         uint8_t *data_idx,
                                         uint8_t length,
                                         bool *is_array,
                                         bool *has_size,
                                         e_type *type) {
    uint8_t typedesc;

    // copy TypeDesc
    // check buffer bound
    if ((*data_idx + sizeof(typedesc)) > length) {
        return false;
    }
    typedesc = data[(*data_idx)++];
    *is_array = typedesc & ARRAY_MASK;
    *has_size = typedesc & TYPESIZE_MASK;
    *type = typedesc & TYPE_MASK;
    // sanity check
    switch (*type) {
        case TYPE_STRUCT:
        case TYPE_SOL_INT:
        case TYPE_SOL_UINT:
        case TYPE_SOL_ADDRESS:
        case TYPE_SOL_BOOL:
        case TYPE_SOL_STRING:
        case TYPE_SOL_BYTES_FIX:
        case TYPE_SOL_BYTES_DYN:
            break;
        default:
            return false;
    }
    return true;
}

/**
 * Set struct field custom typename
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful
 */
static bool v1_set_struct_field_custom_typename(s_struct_712_field *field,
                                                const uint8_t *data,
                                                uint8_t *data_idx,
                                                uint8_t length) {
    uint8_t typename_len;

    // copy custom struct name length
    // check buffer bound
    if ((*data_idx + sizeof(typename_len)) > length) {
        return false;
    }
    typename_len = data[(*data_idx)++];

    // copy name
    // check buffer bound
    if ((*data_idx + typename_len) > length) {
        return false;
    }
    if (!is_valid_identifier(&data[*data_idx], typename_len) ||
        !td_field_set_struct_name(field, &data[*data_idx], typename_len)) {
        return false;
    }
    *data_idx += typename_len;
    return true;
}

/**
 * Set struct field's array levels
 *
 * @param[in] data the field data
 * @param[in] data_idx the data index
 * @return whether it was successful
 */
static bool v1_set_struct_field_array(s_struct_712_field *field,
                                      const uint8_t *data,
                                      uint8_t *data_idx,
                                      uint8_t length) {
    // check buffer bound
    if ((*data_idx + sizeof(field->array_level_count)) > length) {
        return false;
    }
    if (!td_field_set_array_level_count(field, data[(*data_idx)++])) {
        return false;
    }
    for (int idx = 0; idx < field->array_level_count; ++idx) {
        e_array_type arr_type;
        uint8_t arr_size = 0;

        // check buffer bound
        if ((*data_idx + sizeof(field->array_levels[idx].type)) > length) {
            return false;
        }
        arr_type = data[(*data_idx)++];
        switch (arr_type) {
            case ARRAY_DYNAMIC:  // nothing to do
                break;
            case ARRAY_FIXED_SIZE:
                // check buffer bound
                if ((*data_idx + sizeof(field->array_levels[idx].size)) > length) {
                    return false;
                }
                arr_size = data[(*data_idx)++];
                break;
            default:
                // should not be in here :^)
                return false;
        }
        if (!td_field_set_array_level(field, idx, arr_type, arr_size)) {
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
static bool v1_set_struct_field_typesize(s_struct_712_field *field,
                                         const uint8_t *data,
                                         uint8_t *data_idx,
                                         uint8_t length) {
    // copy TypeSize
    // check buffer bound
    if ((*data_idx + sizeof(field->type_size)) > length) {
        return false;
    }
    return td_field_set_type_size(field, data[(*data_idx)++]);
}

/**
 * Set struct field's key name
 *
 * @param[in] data the field data
 * @param[in,out] data_idx the data index
 * @return whether it was successful
 */
static bool v1_set_struct_field_keyname(s_struct_712_field *field,
                                        const uint8_t *data,
                                        uint8_t *data_idx,
                                        uint8_t length) {
    uint8_t keyname_len;

    // copy length
    // check buffer bound
    if ((*data_idx + sizeof(keyname_len)) > length) {
        return false;
    }
    keyname_len = data[(*data_idx)++];

    // copy name
    // check buffer bound
    if ((*data_idx + keyname_len) > length) {
        return false;
    }

    if (!is_valid_identifier(&data[*data_idx], keyname_len) ||
        !td_field_set_key_name(field, &data[*data_idx], keyname_len)) {
        return false;
    }
    *data_idx += keyname_len;
    return true;
}

static bool v1_set_struct_field_internal(s_struct_712_field **new_field_ptr,
                                         uint8_t length,
                                         const uint8_t *data) {
    s_struct_712_field *new_field;
    uint8_t data_idx = 0;

    if ((data == NULL) || (length == 0)) {
        return false;
    }

    if (g_pending_struct == NULL) {
        return false;
    }

    bool is_array;
    bool has_size;
    e_type type;
    if (!v1_set_struct_field_typedesc(data, &data_idx, length, &is_array, &has_size, &type)) {
        return false;
    }

    if ((new_field = td_create_field_def(type)) == NULL) {
        return false;
    }
    *new_field_ptr = new_field;

    // check TypeSize flag in TypeDesc
    if (has_size) {
        // TYPESIZE and TYPE_STRUCT are mutually exclusive
        if (new_field->type == TYPE_STRUCT) {
            return false;
        }

        if (v1_set_struct_field_typesize(new_field, data, &data_idx, length) == false) {
            return false;
        }

    } else if (new_field->type == TYPE_STRUCT) {
        if (v1_set_struct_field_custom_typename(new_field, data, &data_idx, length) == false) {
            return false;
        }
    }

    // Validate the type/size combination: a fixed-size-bytes without a size would
    // serialize as plain "bytes" (identical to dynamic bytes) in the schema/type
    // hashes while being hashed as a static value, so a host could make the review
    // show keccak256(X) while the signature commits to X.
    switch (new_field->type) {
        case TYPE_SOL_INT:
        case TYPE_SOL_UINT:
        case TYPE_SOL_BYTES_FIX:
            if (!has_size || (new_field->type_size == 0) || (new_field->type_size > 32)) {
                return false;
            }
            break;
        case TYPE_SOL_ADDRESS:
        case TYPE_SOL_BOOL:
        case TYPE_SOL_STRING:
        case TYPE_SOL_BYTES_DYN:
            if (has_size) {
                return false;
            }
            break;
        case TYPE_STRUCT:
            // typename handled above, mutually exclusive with a type size
            break;
        default:
            return false;
    }
    if (is_array) {
        if (v1_set_struct_field_array(new_field, data, &data_idx, length) == false) {
            return false;
        }
    }

    if (v1_set_struct_field_keyname(new_field, data, &data_idx, length) == false) {
        return false;
    }

    // check that there is no more
    if (data_idx != length) {
        return false;
    }

    return td_add_struct_field_def(g_pending_struct, new_field);
}

/**
 * Set struct field
 *
 * @param[in] length data length
 * @param[in] data the field data
 * @return whether it was successful
 */
bool v1_set_struct_field(uint8_t length, const uint8_t *data) {
    s_struct_712_field *new_field = NULL;

    if (!v1_set_struct_field_internal(&new_field, length, data)) {
        td_discard_struct_field(new_field);
        return false;
    }
    return true;
}

typedef struct {
    const s_struct_712_field *next;  // next field expecting data
    s_struct_712_value *node;        // VAL_STRUCT or VAL_ARRAY being built
    uint8_t array_remaining;         // >0 = array frame; 0 = struct frame
    // remaining array dimensions to open via v1_set_array before elements are the base type
    uint8_t array_levels_remaining;
} s_build_frame;

static struct {
    s_build_frame stack[TD_MAX_DEPTH];
    uint8_t depth;
} g_build;
static struct {
    s_build_frame stack[TD_MAX_DEPTH];
    uint8_t depth;
} g_backup;
static struct {
    s_struct_712_value *leaf;
    uint16_t filled;
} g_pending_field;

// --- helpers ---

static bool push_frame(const s_struct_712_field *next,
                       s_struct_712_value *node,
                       uint8_t array_remaining,
                       uint8_t array_levels_remaining) {
    if (g_build.depth >= TD_MAX_DEPTH) {
        return false;
    }
    g_build.stack[g_build.depth].next = next;
    g_build.stack[g_build.depth].node = node;
    g_build.stack[g_build.depth].array_remaining = array_remaining;
    g_build.stack[g_build.depth].array_levels_remaining = array_levels_remaining;
    g_build.depth++;
    return true;
}

// Auto-descend into TYPE_STRUCT fields with no array levels, skipping the need for an explicit
// P2_IMPL_ARRAY/P2_IMPL_FIELD
static bool auto_descend(void) {
    while (g_build.depth > 0) {
        s_build_frame *f = &g_build.stack[g_build.depth - 1];
        if (f->next == NULL) break;
        if (f->next->type != TYPE_STRUCT) break;
        if (f->next->array_level_count > 0) break;  // array → wait for P2_IMPL_ARRAY

        const s_struct_712 *nested = td_find_struct(f->next->struct_name);
        if (nested == NULL) {
            return false;
        }

        s_struct_712_value *node;
        if ((node = td_create_struct(nested)) == NULL) {
            return false;
        }
        if (!td_append_child(f->node, node)) {
            td_discard_container_value(node);
            return false;
        }

        if (!push_frame(nested->fields, node, 0, 0)) {
            return false;
        }
    }
    return true;
}

// Advance cursor after a leaf is fully received or an array element is done.
static bool advance(void) {
    while (g_build.depth > 0) {
        s_build_frame *f = &g_build.stack[g_build.depth - 1];

        if (f->array_remaining > 0) {
            f->array_remaining--;
            if (f->array_remaining == 0) {
                // array frame exhausted — pop and let the loop advance the parent naturally
                g_build.depth--;
                continue;
            }
            // more elements: push the next one if this array's base type is reached, else wait for
            // the next P2_IMPL_ARRAY
            if ((f->array_levels_remaining == 0) && (f->next->type == TYPE_STRUCT)) {
                const s_struct_712 *nested = td_find_struct(f->next->struct_name);
                if (nested == NULL) {
                    return false;
                }
                s_struct_712_value *elem;
                if ((elem = td_create_struct(nested)) == NULL) {
                    return false;
                }
                if (!td_append_child(f->node, elem)) {
                    td_discard_container_value(elem);
                    return false;
                }
                if (!push_frame(nested->fields, elem, 0, 0)) {
                    return false;
                }
                return auto_descend();
            }
            return true;  // next leaf/array arrives via next v1_add_field/v1_set_array call
        } else {
            f->next = (const s_struct_712_field *) ((flist_node_t *) f->next)->next;
        }

        if (g_build.depth == 0) {
            return true;
        }
        f = &g_build.stack[g_build.depth - 1];

        if (f->next == NULL) {
            // struct complete — pop and keep advancing
            g_build.depth--;
            continue;
        }

        return auto_descend();
    }
    return true;
}

// --- value tree deinit ---

bool v1_set_root(const char *name) {
    bool ret;
    const s_struct_712 *root;

    // reject if a P2_IMPL_FIELD chunk sequence is still in flight, to avoid orphaning the pending
    // leaf
    if (g_pending_field.leaf != NULL) {
        return false;
    }

    // reject switching roots before all of the previous root's fields have been provided
    if (g_build.depth != 0) {
        return false;
    }

    if ((root = td_find_struct(name)) == NULL) {
        return false;
    }

    s_struct_712_value *node;
    if ((node = td_create_struct(root)) == NULL) {
        return false;
    }

    if (strcmp(name, "EIP712Domain") == 0) {
        g_domain_root = node;
        ret = td_set_domain(node);
    } else {
        ret = td_set_message(node);
    }
    if (!ret) {
        return false;
    }

    if (!push_frame(root->fields, node, 0, 0)) {
        return false;
    }
    return auto_descend();
}

bool v1_set_array(size_t count) {
    // reject if a P2_IMPL_FIELD chunk sequence is still in flight, to avoid desyncing the pending
    // leaf's frames
    if (g_pending_field.leaf != NULL) {
        return false;
    }
    if (g_build.depth == 0) {
        return false;
    }
    s_build_frame *f = &g_build.stack[g_build.depth - 1];

    // Determines the field/remaining-dimensions for this call: continues a pending array frame's
    // next dimension, else starts a fresh field's first dimension
    const s_struct_712_field *elem_field;
    uint8_t levels_remaining_after;

    if ((elem_field = f->next) == NULL) {
        return false;
    }
    if ((f->array_remaining > 0) && (f->array_levels_remaining > 0)) {
        levels_remaining_after = f->array_levels_remaining - 1;
    } else if (f->array_remaining > 0) {
        // leaf-level array still in progress — reject re-entry
        return false;
    } else {
        if (elem_field->array_level_count == 0) {
            return false;
        }
        levels_remaining_after = elem_field->array_level_count - 1;
    }

    // reject a count that doesn't match this dimension's declared fixed size
    uint8_t level_idx = elem_field->array_level_count - 1 - levels_remaining_after;
    const s_struct_712_field_array_level *level = &elem_field->array_levels[level_idx];
    if ((level->type == ARRAY_FIXED_SIZE) && (count != level->size)) {
        return false;
    }

    s_struct_712_value *arr;
    if ((arr = td_create_array(elem_field)) == NULL) {
        return false;
    }
    if (!td_append_child(f->node, arr)) {
        td_discard_container_value(arr);
        return false;
    }

    if (count == 0) {
        // empty array — snapshot only struct-level frames (skip array-container frames, which would
        // duplicate prefix segments) for discarded-path checks
        g_backup.depth = 0;
        for (uint8_t i = 0; i < g_build.depth; ++i) {
            if (g_build.stack[i].array_remaining == 0) {
                g_backup.stack[g_backup.depth++] = g_build.stack[i];
            }
        }
        // advance past this field
        return advance();
    }

    if (count > UINT8_MAX) {
        return false;
    }
    if (!push_frame(elem_field, arr, (uint8_t) count, levels_remaining_after)) {
        return false;
    }

    // Only instantiate the base-type element once every array dimension has been opened, else wait
    // for the next v1_set_array call
    if ((levels_remaining_after == 0) && (elem_field->type == TYPE_STRUCT)) {
        const s_struct_712 *nested = td_find_struct(elem_field->struct_name);
        if (nested == NULL) {
            return false;
        }
        s_struct_712_value *elem;
        if ((elem = td_create_struct(nested)) == NULL) {
            return false;
        }
        if (!td_append_child(arr, elem)) {
            td_discard_container_value(elem);
            return false;
        }
        if (!push_frame(nested->fields, elem, 0, 0)) {
            return false;
        }
        return auto_descend();
    }
    return true;
}

bool v1_is_complete(void) {
    return (g_build.depth == 0) && td_has_domain() && td_has_message();
}

bool v1_domain_complete(void) {
    // domain value tree fully built (build stack fully popped) and message not started yet
    return td_has_domain() && !td_has_message() && (g_build.depth == 0);
}

bool v1_field_streaming(void) {
    return g_pending_field.leaf != NULL;
}

bool v1_current_field_uninstantiated_array(void) {
    if (g_build.depth == 0) {
        return false;
    }
    const s_build_frame *f = &g_build.stack[g_build.depth - 1];
    if (f->array_remaining > 0) {
        // inside an array: uninstantiated while inner dimensions still await P2_IMPL_ARRAY
        return f->array_levels_remaining > 0;
    }
    return (f->next != NULL) && (f->next->array_level_count > 0);
}

const s_struct_712_value *v1_add_field(const uint8_t *data, size_t length, bool more) {
    if (g_build.depth == 0) {
        return NULL;
    }

    s_build_frame *f = &g_build.stack[g_build.depth - 1];

    // Every array level on the current field must have been opened via v1_set_array()
    // before a leaf value is accepted. Otherwise the declared array shape is collapsed
    // and the value is encoded as the base type, letting the host supply directly the
    // 32-byte aggregate that canonical EIP-712 computes for an array it never sent.
    if (v1_current_field_uninstantiated_array()) {
        return NULL;
    }

    if (g_pending_field.leaf == NULL) {
        // first chunk: data starts with uint16_t total_size
        if (length < sizeof(uint16_t)) {
            return NULL;
        }
        uint16_t total_size = read_u16_be(data, 0);
        data += sizeof(uint16_t);
        length -= sizeof(uint16_t);

        if ((f->next->type == TYPE_SOL_ADDRESS) && (total_size != ADDRESS_LENGTH)) {
            PRINTF("Error: address field is %u bytes long\n", total_size);
            return NULL;
        }
        if ((f->next->type == TYPE_SOL_BYTES_FIX) && (total_size != f->next->type_size)) {
            PRINTF("Error: bytes%u field is %u bytes long\n", f->next->type_size, total_size);
            return NULL;
        }

        s_struct_712_value *leaf;
        if ((leaf = td_create_leaf(f->next, total_size)) == NULL) {
            return NULL;
        }

        g_pending_field.leaf = leaf;
        g_pending_field.filled = 0;
    }

    if (!td_leaf_write(g_pending_field.leaf, g_pending_field.filled, data, length)) {
        return NULL;
    }
    g_pending_field.filled += length;

    if (!more) {
        if (!td_leaf_is_complete(g_pending_field.leaf, g_pending_field.filled)) {
            return NULL;
        }
        s_struct_712_value *completed = g_pending_field.leaf;
        // A domain chainId wider than 64 bits with non-zero leading bytes would make the
        // numeric chain ID used by filtering/display diverge from the full bytes the domain
        // hash commits to; zero-padding is tolerated (trimmed by td_get_domain_chain_id)
        if ((f->node == g_domain_root) && (strcmp(f->next->key_name, "chainId") == 0) &&
            (completed->length > sizeof(uint64_t)) &&
            !is_zeroes_buffer(completed->data, completed->length - sizeof(uint64_t))) {
            td_discard_leaf(completed);
            explicit_bzero(&g_pending_field, sizeof(g_pending_field));
            return NULL;
        }
        if (!td_append_child(f->node, completed)) {
            return NULL;
        }
        explicit_bzero(&g_pending_field, sizeof(g_pending_field));
        if (!advance()) {
            return NULL;
        }
        return completed;
    }
    return g_pending_field.leaf;
}

// --- path-compatible accessors (derived from build stack) ---

e_root_type v1_get_root_type(void) {
    if (!td_has_domain()) {
        return ROOT_NONE;
    }
    if (!td_has_message()) {
        return ROOT_DOMAIN;
    }
    return ROOT_MESSAGE;
}

const s_struct_712_field *v1_get_current_field(void) {
    if (g_build.depth == 0) {
        return NULL;
    }
    return g_build.stack[g_build.depth - 1].next;
}

uint8_t v1_depth_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < g_build.depth; ++i) {
        if (g_build.stack[i].array_remaining == 0) count++;
    }
    return count;
}

const s_struct_712_field *v1_nth_field(uint8_t n) {
    if (n == 0) {
        return NULL;
    }
    uint8_t count = 0;
    for (uint8_t i = 0; i < g_build.depth; ++i) {
        if (g_build.stack[i].array_remaining == 0) {
            count++;
            if (count == n) {
                return g_build.stack[i].next;
            }
        }
    }
    return NULL;
}

uint8_t v1_backup_depth_count(void) {
    return g_backup.depth;
}

const s_struct_712_field *v1_backup_nth_field(uint8_t n) {
    if ((n == 0) || (n > g_backup.depth)) {
        return NULL;
    }
    return g_backup.stack[n - 1].next;
}

typedef struct {
    const char *ptr;
    size_t length;
} s_key_slice;

static bool match_key_name(const s_struct_712_field *f, const void *context_ptr) {
    const s_key_slice *context = context_ptr;
    return (context->length == strlen(f->key_name)) &&
           (memcmp(context->ptr, f->key_name, context->length) == 0);
}

bool v1_backup_exists(const char *path, size_t length) {
    size_t offset = 0;
    size_t i;
    const s_struct_712_field *field_ptr;
    const char *typename;
    const s_struct_712 *struct_ptr;

    if (g_backup.depth == 0) {
        return false;
    }
    field_ptr = g_backup.stack[g_backup.depth - 1].next;
    if (field_ptr == NULL) {
        return false;
    }

    while (offset < length) {
        if (((offset + 1) > length) || (memcmp(path + offset, ".", 1) != 0)) {
            return false;
        }
        offset += 1;
        if (((offset + 2) <= length) && (memcmp(path + offset, "[]", 2) == 0)) {
            if (field_ptr->array_level_count == 0) {
                return false;
            }
            offset += 2;
        } else if (offset < length) {
            for (i = 0; ((offset + i) < length) && (path[offset + i] != '.'); ++i);
            typename = td_get_struct_field_typename(field_ptr);
            if ((struct_ptr = td_find_struct(typename)) == NULL) {
                return false;
            }
            field_ptr = td_find_struct_field_if(struct_ptr,
                                                match_key_name,
                                                &(s_key_slice) {path + offset, i});
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

void v1_parse_deinit(void) {
    g_build.depth = 0;
    g_backup.depth = 0;
    td_discard_leaf(g_pending_field.leaf);
    explicit_bzero(&g_pending_field, sizeof(g_pending_field));
    g_pending_struct = NULL;
    g_domain_root = NULL;
}
