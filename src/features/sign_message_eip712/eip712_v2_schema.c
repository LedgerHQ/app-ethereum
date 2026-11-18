#include "tlv_library.h"
#include "app_mem_utils.h"
#include "eip712_v2_schema.h"
#include "eip712_v2_schema_struct.h"

typedef struct {
    flist_node_t _list;
    s_struct_712 *ptr;
} s_struct_node;

typedef struct {
    TLV_reception_t received_tags;
    s_struct_node *struct_nodes;
} s_schema_ctx;

#define SCHEMA_TAGS(X)                                       \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_EIP712_STRUCT, handle_eip712_struct, ALLOW_MULTIPLE_TAG)

static bool handle_version(const tlv_data_t *data, s_schema_ctx *context) {
    uint8_t version;

    (void) context;
    if (!get_uint8_t_from_tlv_data(data, &version)) {
        return false;
    }
    return version == 1;
}

static bool handle_eip712_struct(const tlv_data_t *data, s_schema_ctx *context) {
    s_struct_node *node;

    if ((node = APP_MEM_ALLOC(sizeof(*node))) == NULL) {
        return false;
    }
    explicit_bzero(node, sizeof(*node));
    if (!handle_eip712_v2_schema_struct_struct(&data->value, &node->ptr)) {
        APP_MEM_FREE(node);
        return false;
    }
    flist_push_back((flist_node_t **) &context->struct_nodes, (flist_node_t *) node);
    return true;
}

DEFINE_TLV_PARSER(SCHEMA_TAGS, NULL, schema_tlv_parser)

static bool verify_eip712_v2_schema_struct(const s_schema_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VERSION, TAG_EIP712_STRUCT);
}

static void delete_struct_node(s_struct_node *node) {
    td_discard_struct_def(node->ptr);
    APP_MEM_FREE(node);
}

static bool build_schema(s_schema_ctx *context) {
    for (s_struct_node *node = context->struct_nodes; node != NULL;
         node = (s_struct_node *) ((flist_node_t *) node)->next) {
        // td_add_struct_def() owns the struct whatever the outcome
        s_struct_712 *struct_def = node->ptr;

        node->ptr = NULL;
        if (!td_add_struct_def(struct_def)) {
            flist_clear((flist_node_t **) &context->struct_nodes,
                        (f_list_node_del) &delete_struct_node);
            return false;
        }
    }
    return true;
}

bool handle_eip712_v2_schema_struct(const buffer_t *buf) {
    s_schema_ctx context = {0};
    bool ret;

    ret = schema_tlv_parser(buf, &context, &context.received_tags) &&
          verify_eip712_v2_schema_struct(&context) && build_schema(&context);
    flist_clear((flist_node_t **) &context.struct_nodes, (f_list_node_del) &delete_struct_node);
    return ret;
}
