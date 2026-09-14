#include "typed_data.h"
#include "sol_typenames.h"
#include "eip712_v1_context.h"
#include "common_utils.h"
#include "app_mem_utils.h"
#include "read.h"
#include "os_utils.h"  // is_zeroes_buffer
#include "value_hash.h"

static s_struct_712 *g_structs = NULL;
static s_eip712_impl g_impl;

bool td_init(void) {
    if (g_structs != NULL) {
        td_deinit();
        return false;
    }
    return true;
}

// to be used as a \ref f_list_node_del
static void delete_struct_field_internal(s_struct_712_field *f) {
    if (f->type == TYPE_STRUCT) {
        APP_MEM_FREE(f->struct_name);
    }
    APP_MEM_FREE(f->array_levels);
    APP_MEM_FREE(f->key_name);
    APP_MEM_FREE(f);
}

void td_discard_struct_field(s_struct_712_field *field) {
    if (field != NULL) {
        delete_struct_field_internal(field);
    }
}

// to be used as a \ref f_list_node_del
static void delete_struct(s_struct_712 *s) {
    APP_MEM_FREE(s->name);
    flist_clear((flist_node_t **) &s->fields, (f_list_node_del) &delete_struct_field_internal);
    APP_MEM_FREE(s);
}

static void delete_value(s_struct_712_value *v) {
    if (v->kind == VAL_ATOMIC) {
        APP_MEM_FREE(v->data);
    } else {
        flist_clear((flist_node_t **) &v->children, (f_list_node_del) &delete_value);
    }
    APP_MEM_FREE(v);
}

s_struct_712_value *td_create_leaf(const s_struct_712_field *field, size_t total_size) {
    s_struct_712_value *v;

    if ((v = APP_MEM_ALLOC(sizeof(*v))) != NULL) {
        explicit_bzero(v, sizeof(*v));
        v->kind = VAL_ATOMIC;
        v->field = field;
        v->length = total_size;
        if (v->length > 0) {
            if ((v->data = APP_MEM_ALLOC(v->length)) == NULL) {
                APP_MEM_FREE(v);
                return NULL;
            }
        }
    }
    return v;
}

void td_discard_leaf(s_struct_712_value *leaf) {
    if (leaf == NULL) {
        return;
    }
    APP_MEM_FREE(leaf->data);
    APP_MEM_FREE(leaf);
}

s_struct_712_value *td_create_struct(const s_struct_712 *struct_type) {
    s_struct_712_value *v;

    if ((v = APP_MEM_ALLOC(sizeof(*v))) != NULL) {
        explicit_bzero(v, sizeof(*v));
        v->kind = VAL_STRUCT;
        v->struct_type = struct_type;
    }
    return v;
}

void td_discard_container_value(s_struct_712_value *node) {
    if (node == NULL) {
        return;
    }
    flist_clear((flist_node_t **) &node->children, (f_list_node_del) &delete_value);
    APP_MEM_FREE(node);
}

s_struct_712_value *td_create_array(const s_struct_712_field *field) {
    s_struct_712_value *v;

    if ((v = APP_MEM_ALLOC(sizeof(*v))) != NULL) {
        explicit_bzero(v, sizeof(*v));
        v->kind = VAL_ARRAY;
        v->field = field;
    }
    return v;
}

bool td_append_child(s_struct_712_value *parent, const s_struct_712_value *child) {
    if ((parent == NULL) || (child == NULL)) {
        return false;
    }
    flist_push_back((flist_node_t **) &parent->children, (flist_node_t *) child);
    return true;
}

bool td_leaf_write(s_struct_712_value *leaf, size_t offset, const uint8_t *data, size_t length) {
    if ((leaf == NULL) || (leaf->kind != VAL_ATOMIC)) {
        return false;
    }
    if ((offset + length) > leaf->length) {
        return false;
    }
    // A zero-length leaf has no buffer, and an empty APDU payload has no data.
    if (length > 0) {
        memcpy(&leaf->data[offset], data, length);
    }
    return true;
}

bool td_leaf_is_complete(const s_struct_712_value *leaf, size_t filled) {
    if ((leaf == NULL) || (leaf->kind != VAL_ATOMIC)) {
        return false;
    }
    return filled == leaf->length;
}

s_struct_712_field *td_create_field_def(e_type type) {
    s_struct_712_field *field;

    if ((field = APP_MEM_ALLOC(sizeof(*field))) != NULL) {
        explicit_bzero(field, sizeof(*field));
        field->type = type;
    }
    return field;
}

bool td_field_set_type_size(s_struct_712_field *field, size_t type_size) {
    if (field == NULL) {
        return false;
    }
    if (field->type == TYPE_STRUCT) {
        return false;
    }
    field->type_size = type_size;
    return true;
}

bool td_field_set_struct_name(s_struct_712_field *field, const uint8_t *name, size_t length) {
    if (field == NULL) {
        return false;
    }
    if (field->type != TYPE_STRUCT) {
        return false;
    }
    if (name == NULL) {
        return false;
    }
    if (field->struct_name != NULL) {
        // to prevent a memory leak
        APP_MEM_FREE(field->struct_name);
    }
    if ((field->struct_name = APP_MEM_ALLOC(length + 1)) == NULL) {
        return false;
    }
    memcpy(field->struct_name, name, length);
    field->struct_name[length] = '\0';
    return true;
}

bool td_field_set_array_level_count(s_struct_712_field *field, uint8_t count) {
    if (field == NULL) {
        return false;
    }
    if (field->array_levels != NULL) {
        // to prevent a memory leak
        APP_MEM_FREE(field->array_levels);
    }
    if ((field->array_levels = APP_MEM_ALLOC(sizeof(*field->array_levels) * count)) == NULL) {
        return false;
    }
    explicit_bzero(field->array_levels, sizeof(*field->array_levels) * count);
    field->array_level_count = count;
    return true;
}

bool td_field_set_array_level(s_struct_712_field *field,
                              uint8_t index,
                              e_array_type type,
                              uint8_t size) {
    if (field == NULL) {
        return false;
    }
    if (index >= field->array_level_count) {
        return false;
    }
    field->array_levels[index].type = type;
    if (type == ARRAY_FIXED_SIZE) {
        field->array_levels[index].size = size;
    }
    return true;
}

bool td_field_set_key_name(s_struct_712_field *field, const uint8_t *name, size_t length) {
    if (field == NULL) {
        return false;
    }
    if (name == NULL) {
        return false;
    }
    if (field->key_name != NULL) {
        // to prevent a memory leak
        APP_MEM_FREE(field->key_name);
    }
    if ((field->key_name = APP_MEM_ALLOC(length + 1)) == NULL) {
        return false;
    }
    memcpy(field->key_name, name, length);
    field->key_name[length] = '\0';
    return true;
}

void td_deinit(void) {
    if (g_impl.domain != NULL) {
        delete_value(g_impl.domain);
        g_impl.domain = NULL;
    }
    if (g_impl.message != NULL) {
        delete_value(g_impl.message);
        g_impl.message = NULL;
    }
    flist_clear((flist_node_t **) &g_structs, (f_list_node_del) &delete_struct);
}

const char *td_get_struct_field_typename(const s_struct_712_field *field_ptr) {
    if (field_ptr == NULL) {
        return NULL;
    }
    if (field_ptr->type == TYPE_STRUCT) {
        return field_ptr->struct_name;
    }
    return get_struct_field_sol_typename(field_ptr);
}

static bool match_struct_name(const s_struct_712 *node, const void *context_ptr) {
    return (node->name != NULL) && (strcmp(node->name, (const char *) context_ptr) == 0);
}

const s_struct_712 *td_find_struct(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    return td_find_struct_if(match_struct_name, (const void *) name);
}

s_struct_712 *td_create_struct_def(const char *name) {
    s_struct_712 *def;
    size_t length;

    if (name == NULL) {
        return NULL;
    }
    length = strlen(name);

    if ((def = APP_MEM_ALLOC(sizeof(*def))) == NULL) {
        return NULL;
    }
    explicit_bzero(def, sizeof(*def));

    if ((def->name = APP_MEM_ALLOC(length + 1)) == NULL) {
        APP_MEM_FREE(def);
        return NULL;
    }
    memcpy(def->name, name, length);
    def->name[length] = '\0';
    return def;
}

bool td_add_struct_def(const s_struct_712 *struct_def) {
    if (struct_def == NULL) {
        return false;
    }
    flist_push_back((flist_node_t **) &g_structs, (flist_node_t *) struct_def);
    return true;
}

bool td_add_struct_field_def(s_struct_712 *struct_def, const s_struct_712_field *field_def) {
    if (struct_def == NULL) {
        return false;
    }
    flist_push_back((flist_node_t **) &struct_def->fields, (flist_node_t *) field_def);
    return true;
}

bool td_set_domain(s_struct_712_value *node) {
    if (node == NULL) {
        return false;
    }
    if (g_impl.domain != NULL) {
        APP_MEM_FREE(node);
        return false;
    }
    if (node->kind != VAL_STRUCT) {
        APP_MEM_FREE(node);
        return false;
    }
    if (strcmp(node->struct_type->name, "EIP712Domain") != 0) {
        APP_MEM_FREE(node);
        return false;
    }
    g_impl.domain = node;
    return true;
}

bool td_set_message(s_struct_712_value *node) {
    if (node == NULL) {
        return false;
    }
    if (g_impl.message != NULL) {
        APP_MEM_FREE(node);
        return false;
    }
    if (node->kind != VAL_STRUCT) {
        APP_MEM_FREE(node);
        return false;
    }
    g_impl.message = node;
    return true;
}

bool td_has_domain(void) {
    return g_impl.domain != NULL;
}

bool td_has_message(void) {
    return g_impl.message != NULL;
}

bool td_get_domain_chain_id(uint64_t *chain_id) {
    if (g_impl.domain != NULL) {
        for (const s_struct_712_value *child = g_impl.domain->children; child != NULL;
             child = (const s_struct_712_value *) ((const flist_node_t *) child)->next) {
            if ((child->kind == VAL_ATOMIC) && (strcmp(child->field->key_name, "chainId") == 0)) {
                const uint8_t *data = child->data;
                uint16_t length = child->length;

                // tolerate zero-left-padded values (the domain hash commits to the full bytes,
                // the numeric value must stay identical); non-zero padding is rejected at
                // ingestion by the value parser
                if (length > sizeof(*chain_id)) {
                    uint16_t leading = length - sizeof(*chain_id);
                    if (!is_zeroes_buffer(data, leading)) {
                        return false;
                    }
                    data += leading;
                    length = sizeof(*chain_id);
                }
                *chain_id = u64_from_BE(data, (uint8_t) length);
                return true;
            }
        }
    }
    return false;
}

bool td_get_domain_contract_addr(uint8_t addr[ADDRESS_LENGTH]) {
    const char *ethermint_vc = "cosmos";

    if (g_impl.domain != NULL) {
        for (const s_struct_712_value *child = g_impl.domain->children; child != NULL;
             child = (const s_struct_712_value *) ((const flist_node_t *) child)->next) {
            if ((child->kind == VAL_ATOMIC) &&
                (strcmp(child->field->key_name, "verifyingContract") == 0)) {
                const uint8_t *data = child->data;
                uint16_t length = child->length;

                switch (child->field->type) {
                    case TYPE_SOL_ADDRESS:
                        if (length != ADDRESS_LENGTH) {
                            PRINTF("Error: verifyingContract is %u bytes long\n", length);
                            return false;
                        }
                        break;
                    case TYPE_SOL_STRING:
                        if ((length != strlen(ethermint_vc)) ||
                            (strncmp((char *) data, ethermint_vc, length) != 0)) {
                            PRINTF("Error: non standard verifyingContract\n");
                            return false;
                        }
                        break;
                    default:
                        PRINTF("Error: unexpected type for verifyingContract (%u)!\n",
                               child->field->type);
                        return false;
                }
                memcpy(addr, data, length);
                explicit_bzero(addr + length, ADDRESS_LENGTH - length);
                return true;
            }
        }
    }
    return false;
}

/**
 * Recursive helper for tree traversal. Visits node, then recurses on children if
 * VAL_STRUCT/VAL_ARRAY.
 * @return false if visitor returned false (abort), true otherwise
 */
static bool traverse_node(const s_struct_712_value *node,
                          td_f_value_visitor visitor,
                          void *context) {
    if (node == NULL) {
        return true;
    }

    // Visit this node
    if (!visitor(node, context)) {
        return false;
    }

    // Recurse on children for composite types
    if ((node->kind == VAL_STRUCT) || (node->kind == VAL_ARRAY)) {
        for (const s_struct_712_value *child = node->children; child != NULL;
             child = (const s_struct_712_value *) ((const flist_node_t *) child)->next) {
            if (!traverse_node(child, visitor, context)) {
                return false;
            }
        }
    }

    return true;
}

bool td_traverse_domain(td_f_value_visitor visitor, void *context) {
    if (visitor == NULL) {
        return false;
    }
    return traverse_node(g_impl.domain, visitor, context);
}

bool td_traverse_message(td_f_value_visitor visitor, void *context) {
    if (visitor == NULL) {
        return false;
    }
    return traverse_node(g_impl.message, visitor, context);
}

void td_release_leaf_value(const s_struct_712_value *node) {
    if ((node == NULL) || (node->kind != VAL_ATOMIC)) {
        return;
    }
    // s_struct_712_value is exposed as const to traversal visitors, but this
    // function's whole purpose is to mutate it; cast away constness here only.
    s_struct_712_value *mutable_node = (s_struct_712_value *) node;
    APP_MEM_FREE(mutable_node->data);
    mutable_node->data = NULL;
    mutable_node->length = 0;
}

bool td_visit_structs(td_f_struct_visitor visitor, void *context) {
    const s_struct_712 *it = g_structs;

    while (it != NULL) {
        if (!visitor(it, context)) {
            return false;
        }
        it = (const s_struct_712 *) ((const flist_node_t *) it)->next;
    }
    return true;
}

bool td_visit_struct_fields(const s_struct_712 *s,
                            td_f_struct_field_visitor visitor,
                            void *context) {
    const s_struct_712_field *it;

    if (s != NULL) {
        it = s->fields;
        while (it != NULL) {
            if (!visitor(it, context)) {
                return false;
            }
            it = (const s_struct_712_field *) ((const flist_node_t *) it)->next;
        }
    }
    return true;
}

bool td_hash_pass(void) {
    return value_hash_pass(&g_impl);
}

const s_struct_712 *td_find_struct_if(td_f_struct_pred pred, const void *ctx) {
    const s_struct_712 *struct_ptr = g_structs;

    while (struct_ptr != NULL) {
        if (pred(struct_ptr, ctx)) {
            return struct_ptr;
        }
        struct_ptr = (const s_struct_712 *) ((const flist_node_t *) struct_ptr)->next;
    }
    return NULL;
}

const s_struct_712_field *td_find_struct_field_if(const s_struct_712 *s,
                                                  td_f_struct_field_pred pred,
                                                  const void *ctx) {
    const s_struct_712_field *field_ptr;

    if (s != NULL) {
        field_ptr = s->fields;
        while (field_ptr != NULL) {
            if (pred(field_ptr, ctx)) {
                return field_ptr;
            }
            field_ptr = (const s_struct_712_field *) ((const flist_node_t *) field_ptr)->next;
        }
    }
    return NULL;
}
