#include "tlv_library.h"
#include "os_print.h"
#include "app_mem_utils.h"
#include "bip32.h"
#include "shared_context.h"
#include "eip712_v2_values.h"
#include "typed_data.h"

typedef struct {
    TLV_reception_t received_tags;
    s_struct_712_value *node;  // container being filled
    uint8_t levels_remaining;  // array dimensions left to open before the base type
    uint8_t depth;             // nesting levels already open above this sequence
    uint8_t value_levels;      // dimensions left at the value being handled
} s_value_seq_ctx;

// One entry per struct field or per array element, in schema-declared order
#define VALUE_SEQ_TAGS(X)                              \
    X(0x00, TAG_LEAF, handle_leaf, ALLOW_MULTIPLE_TAG) \
    X(0x01, TAG_SEQ, handle_seq, ALLOW_MULTIPLE_TAG)

// the value sequence parser recurses through the handlers of the values it holds
static bool handle_value_seq_struct(const buffer_t *buf,
                                    s_struct_712_value *node,
                                    uint8_t levels_remaining,
                                    uint8_t depth);

/**
 * Array dimensions still to be opened at the sequence's current position
 *
 * Inside a struct each value starts at its own field's outermost dimension; inside an
 * array the count is whatever the enclosing dimension left to open.
 *
 * @param[in] context the value sequence context
 * @param[in] field field the value must conform to
 * @return number of dimensions left to open
 */
static uint8_t levels_at_position(const s_value_seq_ctx *context, const s_struct_712_field *field) {
    if (context->node->kind == VAL_STRUCT) {
        return field->array_level_count;
    }
    return context->levels_remaining;
}

/**
 * Whether a value nests a sequence rather than holding raw bytes
 *
 * Derived from the schema alone: array dimensions are opened first, one sequence per
 * dimension, then a struct-typed field opens one more for its instance.
 *
 * @param[in] field field the value must conform to
 * @param[in] value_levels dimensions left to open at the value's position
 * @return whether the value is a nested sequence
 */
static bool is_seq(const s_struct_712_field *field, uint8_t value_levels) {
    return (value_levels > 0) || (field->type == TYPE_STRUCT);
}

/**
 * Common handler rejecting any value whose kind contradicts the schema
 *
 * Runs before every tag handler and aborts the whole parse on failure, so each handler may
 * rely on it having established that a field is still expected at this position, that the
 * received tag matches that field's kind, and that \ref value_levels describes the value
 * about to be handled.
 *
 * @param[in] data the tlv data
 * @param[in] context the value sequence context
 * @return whether the value is expected at this position
 */
static bool value_seq_common_handler(const tlv_data_t *data, s_value_seq_ctx *context);

static bool handle_leaf(const tlv_data_t *data, s_value_seq_ctx *context) {
    // the common handler ran first, so a field is left and its kind matches this tag
    const s_struct_712_field *field = td_value_expected_field(context->node);
    s_struct_712_value *leaf;

    if ((leaf = td_create_leaf(field, data->value.size)) == NULL) {
        return false;
    }
    if (!td_leaf_write(leaf, 0, data->value.ptr, data->value.size)) {
        td_discard_leaf(leaf);
        return false;
    }
    if (!td_append_child(context->node, leaf)) {
        td_discard_leaf(leaf);
        return false;
    }
    return true;
}

static bool handle_seq(const tlv_data_t *data, s_value_seq_ctx *context) {
    // the common handler ran first, so a field is left, its kind matches this tag, and
    // value_levels describes this value's position
    const s_struct_712_field *field = td_value_expected_field(context->node);
    uint8_t levels_below;
    s_struct_712_value *seq;

    if (context->value_levels > 0) {
        if ((seq = td_create_array(field)) == NULL) {
            return false;
        }
        // elements of this dimension are the base type once every dimension has been opened
        levels_below = context->value_levels - 1;
    } else {
        const s_struct_712 *struct_type;

        // a value with no dimension left to open nests only to hold a struct instance
        if (field->type != TYPE_STRUCT) {
            return false;
        }
        if ((struct_type = td_find_struct(td_get_struct_field_typename(field))) == NULL) {
            return false;
        }
        if ((seq = td_create_struct(struct_type)) == NULL) {
            return false;
        }
        levels_below = 0;
    }
    if (!handle_value_seq_struct(&data->value, seq, levels_below, context->depth + 1)) {
        td_discard_container_value(seq);
        return false;
    }
    if (!td_append_child(context->node, seq)) {
        td_discard_container_value(seq);
        return false;
    }
    return true;
}

DEFINE_TLV_PARSER(VALUE_SEQ_TAGS, &value_seq_common_handler, value_seq_tlv_parser)

static bool value_seq_common_handler(const tlv_data_t *data, s_value_seq_ctx *context) {
    const s_struct_712_field *field = td_value_expected_field(context->node);

    // no field left means more values were sent than the schema declares
    if (field == NULL) {
        return false;
    }
    context->value_levels = levels_at_position(context, field);
    return data->tag == (is_seq(field, context->value_levels) ? TAG_SEQ : TAG_LEAF);
}

/**
 * Check a sequence holds exactly the values the schema declares
 *
 * @param[in] context the value sequence context
 * @return whether the sequence is complete
 */
static bool verify_value_seq_struct(const s_value_seq_ctx *context) {
    const s_struct_712_field *field;

    if (context->node->kind == VAL_STRUCT) {
        // every declared field must have been given a value
        return td_value_expected_field(context->node) == NULL;
    }

    // a node holding values whose dimensions-left-to-open is N is itself dimension N,
    // the schema declaring dimensions innermost first
    field = context->node->field;
    if (context->levels_remaining >= field->array_level_count) {
        return false;
    }
    if (field->array_levels[context->levels_remaining].type == ARRAY_FIXED_SIZE) {
        return td_value_child_count(context->node) ==
               field->array_levels[context->levels_remaining].size;
    }
    return true;
}

static bool handle_value_seq_struct(const buffer_t *buf,
                                    s_struct_712_value *node,
                                    uint8_t levels_remaining,
                                    uint8_t depth) {
    s_value_seq_ctx context = {0};

    // every nested sequence costs a stack frame, so the payload must not drive the depth
    if (depth >= TD_MAX_DEPTH) {
        PRINTF("EIP712 v2: value tree deeper than %d\n", TD_MAX_DEPTH);
        return false;
    }
    context.node = node;
    context.levels_remaining = levels_remaining;
    context.depth = depth;
    if (!value_seq_tlv_parser(buf, &context, &context.received_tags)) {
        return false;
    }
    return verify_value_seq_struct(&context);
}

typedef struct {
    TLV_reception_t received_tags;
    char primary_type[TD_MAX_STRUCT_NAME_LENGTH + 1];
    bip32_path_t deriv_path;
} s_values_ctx;

#define EIP712_VALUES_TAGS(X)                                                \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)                 \
    X(0x01, TAG_PRIMARY_TYPE, handle_primary_type, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_DERIVATION_PATH, handle_derivation_path, ENFORCE_UNIQUE_TAG) \
    X(0x03, TAG_EIP712_DOMAIN, handle_eip712_domain, ENFORCE_UNIQUE_TAG)     \
    X(0x04, TAG_EIP712_MESSAGE, handle_eip712_message, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_values_ctx *context) {
    uint8_t version;

    (void) context;
    if (!get_uint8_t_from_tlv_data(data, &version)) {
        return false;
    }
    return version == 1;
}

static bool handle_primary_type(const tlv_data_t *data, s_values_ctx *context) {
    if (!get_string_from_tlv_data(data, context->primary_type, 1, sizeof(context->primary_type))) {
        return false;
    }
    return true;
}

static bool handle_derivation_path(const tlv_data_t *data, s_values_ctx *context) {
    size_t count;

    if ((data->value.size == 0) || ((data->value.size % sizeof(*context->deriv_path.path)) != 0)) {
        return false;
    }
    count = data->value.size / sizeof(*context->deriv_path.path);
    // checked before narrowing into the uint8_t length
    if (count > ARRAYLEN(context->deriv_path.path)) {
        return false;
    }
    context->deriv_path.length = count;
    if (!bip32_path_read(data->value.ptr,
                         data->value.size,
                         context->deriv_path.path,
                         context->deriv_path.length)) {
        return false;
    }
    return true;
}

/**
 * Build a root value tree and hand it over to the typed data module
 *
 * @param[in] data the tlv data holding the root sequence
 * @param[in] type_name name of the struct type the root is an instance of
 * @param[in] setter typed data function taking ownership of the built root
 * @return whether the tree was built and accepted
 */
static bool handle_root(const tlv_data_t *data,
                        const char *type_name,
                        bool (*setter)(s_struct_712_value *)) {
    const s_struct_712 *struct_type;
    s_struct_712_value *root;

    if ((struct_type = td_find_struct(type_name)) == NULL) {
        return false;
    }
    if ((root = td_create_struct(struct_type)) == NULL) {
        return false;
    }
    // the root sequence is the first nesting level of the tree
    if (!handle_value_seq_struct(&data->value, root, 0, 0)) {
        td_discard_container_value(root);
        return false;
    }
    // the setter takes ownership of the root, whether it accepts it or not
    return setter(root);
}

static bool handle_eip712_domain(const tlv_data_t *data, s_values_ctx *context) {
    (void) context;
    return handle_root(data, TD_DOMAIN_STRUCT_NAME, td_set_domain);
}

// forward declaration: uses TAG_PRIMARY_TYPE, only declared by DEFINE_TLV_PARSER below
static bool handle_eip712_message(const tlv_data_t *data, s_values_ctx *context);

DEFINE_TLV_PARSER(EIP712_VALUES_TAGS, NULL, eip712_values_tlv_parser)

static bool handle_eip712_message(const tlv_data_t *data, s_values_ctx *context) {
    // the message root's type is named by PRIMARY_TYPE, so it must have been received first
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_PRIMARY_TYPE)) {
        return false;
    }
    return handle_root(data, context->primary_type, td_set_message);
}

static bool verify_eip712_v2_values_struct(const s_values_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                   TAG_VERSION,
                                   TAG_PRIMARY_TYPE,
                                   TAG_DERIVATION_PATH,
                                   TAG_EIP712_DOMAIN,
                                   TAG_EIP712_MESSAGE);
}

static bool is_unresolved_reference(const s_struct_712_field *field, const void *context) {
    (void) context;
    return (field->type == TYPE_STRUCT) &&
           (td_find_struct(td_get_struct_field_typename(field)) == NULL);
}

static bool has_unresolved_reference(const s_struct_712 *struct_def, const void *context) {
    return td_find_struct_field_if(struct_def, &is_unresolved_reference, context) != NULL;
}

bool handle_eip712_v2_values_struct(const buffer_t *buf) {
    s_values_ctx context = {0};

    // every struct the schema refers to is defined, whether the values hold one of it or not
    if (td_find_struct_if(&has_unresolved_reference, NULL) != NULL) {
        return false;
    }
    if (!eip712_values_tlv_parser(buf, &context, &context.received_tags)) {
        return false;
    }
    if (!verify_eip712_v2_values_struct(&context)) {
        return false;
    }
    // only commit the signing path once the whole payload has been accepted
    tmpCtx.messageSigningContext712.bip32 = context.deriv_path;
    return true;
}
