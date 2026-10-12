#include <string.h>
#include "tlv_library.h"
#include "lists.h"
#include "app_mem_utils.h"
#include "eip712_v2_schema_struct.h"
#include "eip712_v2_schema_field.h"

typedef struct {
    flist_node_t _list;
    s_struct_712_field *ptr;
} s_field_node;

typedef struct {
    TLV_reception_t received_tags;
    buffer_t name;
    s_field_node *field_nodes;
} s_struct_712_ctx;

#define STRUCT_712_TAGS(X)                                   \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_NAME, handle_name, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_EIP712_FIELD, handle_eip712_field, ALLOW_MULTIPLE_TAG)

static bool handle_version(const tlv_data_t *data, s_struct_712_ctx *context) {
    uint8_t version;

    (void) context;
    if (!get_uint8_t_from_tlv_data(data, &version)) {
        return false;
    }
    return version == 1;
}

static bool handle_name(const tlv_data_t *data, s_struct_712_ctx *context) {
    if ((data->value.size > TD_MAX_STRUCT_NAME_LENGTH) ||
        !td_is_valid_struct_name(data->value.ptr, data->value.size)) {
        return false;
    }
    memcpy(&context->name, &data->value, sizeof(context->name));
    return true;
}

static bool handle_eip712_field(const tlv_data_t *data, s_struct_712_ctx *context) {
    s_field_node *node;

    if ((node = APP_MEM_ALLOC(sizeof(*node))) == NULL) {
        return false;
    }
    explicit_bzero(node, sizeof(*node));
    if (!handle_eip712_v2_schema_field_struct(&data->value, &node->ptr)) {
        APP_MEM_FREE(node);
        return false;
    }
    flist_push_back((flist_node_t **) &context->field_nodes, (flist_node_t *) node);
    return true;
}

DEFINE_TLV_PARSER(STRUCT_712_TAGS, NULL, struct_712_tlv_parser)

static bool has_field_named(const s_struct_712 *struct_def, const char *key_name) {
    for (const s_struct_712_field *f = struct_def->fields; f != NULL;
         f = (const s_struct_712_field *) ((const flist_node_t *) f)->next) {
        if (strcmp(f->key_name, key_name) == 0) {
            return true;
        }
    }
    return false;
}

static s_struct_712 *build_struct(s_struct_712_ctx *context) {
    s_struct_712 *struct_ptr;

    if ((struct_ptr = td_create_struct_def(context->name.ptr, context->name.size)) == NULL) {
        return NULL;
    }
    for (s_field_node *node = context->field_nodes; node != NULL;
         node = (s_field_node *) ((flist_node_t *) node)->next) {
        // field names are the keys of the message, so they must be unique within a struct
        if (has_field_named(struct_ptr, node->ptr->key_name) ||
            !td_add_struct_field_def(struct_ptr, node->ptr)) {
            td_discard_struct_def(struct_ptr);
            return NULL;
        }
        node->ptr = NULL;
    }
    return struct_ptr;
}

// EIP-712 allows a struct without member, such as an empty EIP712Domain
static bool verify_eip712_v2_schema_struct_struct(const s_struct_712_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VERSION, TAG_NAME);
}

static void delete_field_node(s_field_node *node) {
    td_discard_struct_field(node->ptr);
    APP_MEM_FREE(node);
}

bool handle_eip712_v2_schema_struct_struct(const buffer_t *buf, s_struct_712 **out) {
    s_struct_712_ctx context = {0};
    bool ret;

    ret = struct_712_tlv_parser(buf, &context, &context.received_tags) &&
          verify_eip712_v2_schema_struct_struct(&context) &&
          ((*out = build_struct(&context)) != NULL);
    flist_clear((flist_node_t **) &context.field_nodes, (f_list_node_del) &delete_field_node);
    return ret;
}
