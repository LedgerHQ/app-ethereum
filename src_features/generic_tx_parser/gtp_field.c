#ifdef HAVE_GENERIC_TX_PARSER

#include <string.h>
#include "gtp_field.h"
#include "utils.h"
#include "os_print.h"

enum {
    BIT_VERSION = 0,
    BIT_NAME,
    BIT_PARAM_TYPE,
    BIT_PARAM,
};

enum {
    TAG_VERSION = 0x00,
    TAG_NAME = 0x01,
    PARAM_TYPE = 0x02,
    PARAM = 0x03,
};

typedef union {
    s_param_raw_context raw_ctx;
    s_param_amount_context amount_ctx;
} u_param_context;

static bool handle_version(const s_tlv_data *data, s_field_ctx *context) {
    if (data->length != sizeof(context->field->version)) {
        return false;
    }
    context->field->version = data->value[0];
    context->set_flags |= SET_BIT(BIT_VERSION);
    return true;
}

static bool handle_name(const s_tlv_data *data, s_field_ctx *context) {
    str_cpy_explicit_trunc((const char *) data->value,
                           data->length,
                           context->field->name,
                           sizeof(context->field->name));
    context->set_flags |= SET_BIT(BIT_NAME);
    return true;
}

static bool handle_param_type(const s_tlv_data *data, s_field_ctx *context) {
    if (data->length != sizeof(context->field->param_type)) {
        return false;
    }
    if (context->set_flags & SET_BIT(BIT_PARAM_TYPE)) {
        PRINTF("Error: More than one PARAM_TYPE in a FIELD struct!\n");
        return false;
    }
    context->field->param_type = data->value[0];
    switch (context->field->param_type) {
        case PARAM_TYPE_RAW:
        case PARAM_TYPE_AMOUNT:
            break;
        default:
            PRINTF("Error: Unsupported param type (%u)\n", context->field->param_type);
            return false;
    }
    context->set_flags |= SET_BIT(BIT_PARAM_TYPE);
    return true;
}

static bool handle_param(const s_tlv_data *data, s_field_ctx *context) {
    f_tlv_data_handler handler;
    u_param_context param_ctx = {0};

    if (context->set_flags & SET_BIT(BIT_PARAM)) {
        PRINTF("Error: More than one PARAM in a FIELD struct!\n");
        return false;
    }
    if (!(context->set_flags & SET_BIT(BIT_PARAM_TYPE))) {
        PRINTF("Error: Received PARAM without a previous PARAM_TYPE!\n");
        return false;
    }
    switch (context->field->param_type) {
        case PARAM_TYPE_RAW:
            handler = (f_tlv_data_handler) &handle_param_raw_struct;
            param_ctx.raw_ctx.param = &context->field->param_raw;
            break;
        case PARAM_TYPE_AMOUNT:
            handler = (f_tlv_data_handler) &handle_param_amount_struct;
            param_ctx.amount_ctx.param = &context->field->param_amount;
            break;
        default:
            return false;
    }
    context->set_flags |= SET_BIT(BIT_PARAM);
    return tlv_parse(data->value, data->length, handler, &param_ctx);
}

bool handle_field_struct(const s_tlv_data *data, s_field_ctx *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_NAME:
            ret = handle_name(data, context);
            break;
        case PARAM_TYPE:
            ret = handle_param_type(data, context);
            break;
        case PARAM:
            ret = handle_param(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool verify_field_struct(const s_field_ctx *context) {
    uint8_t required_bits = 0;

    // check if struct version was provided
    required_bits |= SET_BIT(BIT_VERSION);
    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }

    // verify required fields
    switch (context->field->version) {
        case 1:
            required_bits |= SET_BIT(BIT_NAME) | SET_BIT(BIT_PARAM_TYPE) | SET_BIT(BIT_PARAM);
            break;
        default:
            PRINTF("Error: unsupported field struct version (%u)\n", context->field->version);
            return false;
    }

    if ((context->set_flags & required_bits) != required_bits) {
        PRINTF("Error: missing required field(s)\n");
        return false;
    }
    return true;
}

bool format_field(const s_field *field) {
    bool ret;

    switch (field->param_type) {
        case PARAM_TYPE_RAW:
            ret = format_param_raw(&field->param_raw, field->name);
            break;
        case PARAM_TYPE_AMOUNT:
            ret = format_param_amount(&field->param_amount, field->name);
            break;
        default:
            ret = false;
    }
    return ret;
}

#endif  // HAVE_GENERIC_TX_PARSER
