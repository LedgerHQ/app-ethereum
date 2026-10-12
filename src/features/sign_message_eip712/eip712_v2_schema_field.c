#include <string.h>
#include "tlv_library.h"
#include "eip712_v2_schema_field.h"
#include "typed_data.h"

// One decoded ARRAY_DIM entry, staged until the field's dimension count is known
typedef struct {
    e_array_type kind;
    uint8_t size;
} s_array_dim;

typedef struct {
    TLV_reception_t received_tags;
    buffer_t name;
    e_type type;
    uint8_t type_size;
    uint8_t dim_count;
    s_array_dim dims[TD_MAX_ARRAY_LEVELS];
    buffer_t struct_name;
} s_field_712_ctx;

#define FIELD_712_TAGS(X)                                        \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)     \
    X(0x01, TAG_NAME, handle_name, ENFORCE_UNIQUE_TAG)           \
    X(0x02, TAG_TYPE, handle_type, ENFORCE_UNIQUE_TAG)           \
    X(0x03, TAG_TYPE_SIZE, handle_type_size, ENFORCE_UNIQUE_TAG) \
    X(0x04, TAG_ARRAY_DIM, handle_array_dim, ALLOW_MULTIPLE_TAG) \
    X(0x05, TAG_STRUCT_NAME, handle_struct_name, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_field_712_ctx *context) {
    uint8_t version;

    (void) context;
    if (!get_uint8_t_from_tlv_data(data, &version)) {
        return false;
    }
    return version == 1;
}

static bool handle_name(const tlv_data_t *data, s_field_712_ctx *context) {
    if (!td_is_valid_field_name(data->value.ptr, data->value.size)) {
        return false;
    }
    memcpy(&context->name, &data->value, sizeof(context->name));
    return true;
}

static bool handle_type(const tlv_data_t *data, s_field_712_ctx *context) {
    uint8_t type;

    if (!get_uint8_t_from_tlv_data(data, &type)) {
        return false;
    }
    switch (type) {
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
    context->type = type;
    return true;
}

static bool handle_type_size(const tlv_data_t *data, s_field_712_ctx *context) {
    uint8_t type_size;

    if (!get_uint8_t_from_tlv_data(data, &type_size)) {
        return false;
    }
    context->type_size = type_size;
    return true;
}

static bool handle_array_dim(const tlv_data_t *data, s_field_712_ctx *context) {
    s_array_dim *dim;
    uint32_t size;

    if (context->dim_count >= ARRAYLEN(context->dims)) {
        return false;
    }
    dim = &context->dims[context->dim_count];

    // an empty payload marks the dimension as dynamic; its presence marks it as fixed
    if (data->value.size == 0) {
        dim->kind = ARRAY_DYNAMIC;
    } else {
        if (!get_uint32_t_from_tlv_data(data, &size)) {
            return false;
        }
        if (size > UINT8_MAX) {
            return false;
        }
        dim->kind = ARRAY_FIXED_SIZE;
        dim->size = (uint8_t) size;
    }
    context->dim_count += 1;
    return true;
}

static bool handle_struct_name(const tlv_data_t *data, s_field_712_ctx *context) {
    if ((data->value.size > TD_MAX_STRUCT_NAME_LENGTH) ||
        !td_is_valid_struct_name(data->value.ptr, data->value.size)) {
        return false;
    }
    memcpy(&context->struct_name, &data->value, sizeof(context->struct_name));
    return true;
}

DEFINE_TLV_PARSER(FIELD_712_TAGS, NULL, field_712_tlv_parser)

static bool verify_eip712_v2_schema_field_struct(const s_field_712_ctx *context) {
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_VERSION, TAG_NAME, TAG_TYPE)) {
        return false;
    }
    switch (context->type) {
        case TYPE_SOL_INT:
        case TYPE_SOL_UINT:
        case TYPE_SOL_BYTES_FIX:
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_TYPE_SIZE)) {
                return false;
            }
            break;
        case TYPE_STRUCT:
        case TYPE_SOL_ADDRESS:
        case TYPE_SOL_BOOL:
        case TYPE_SOL_STRING:
        case TYPE_SOL_BYTES_DYN:
            if (TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_TYPE_SIZE)) {
                return false;
            }
            break;
        default:
            return false;
    }
    if (TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_STRUCT_NAME) !=
        (context->type == TYPE_STRUCT)) {
        return false;
    }
    return true;
}

static bool build_field(const s_field_712_ctx *context, s_struct_712_field *field) {
    if (!td_field_set_key_name(field, context->name.ptr, context->name.size)) {
        return false;
    }
    if (TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_TYPE_SIZE)) {
        if (!td_field_set_type_size(field, context->type_size)) {
            return false;
        }
    }
    if (TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_ARRAY_DIM)) {
        if (!td_field_set_array_level_count(field, context->dim_count)) {
            return false;
        }
        for (int i = 0; i < context->dim_count; ++i) {
            if (!td_field_set_array_level(field, i, context->dims[i].kind, context->dims[i].size)) {
                return false;
            }
        }
    }
    if (TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_STRUCT_NAME)) {
        if (!td_field_set_struct_name(field, context->struct_name.ptr, context->struct_name.size)) {
            return false;
        }
    }
    return true;
}

bool handle_eip712_v2_schema_field_struct(const buffer_t *buf, s_struct_712_field **out) {
    s_field_712_ctx context = {0};
    s_struct_712_field *field;

    if (!field_712_tlv_parser(buf, &context, &context.received_tags)) {
        return false;
    }
    if (!verify_eip712_v2_schema_field_struct(&context)) {
        return false;
    }
    if ((field = td_create_field_def(context.type)) == NULL) {
        return false;
    }
    if (!build_field(&context, field)) {
        td_discard_struct_field(field);
        return false;
    }
    *out = field;
    return true;
}
