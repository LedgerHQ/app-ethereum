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
    s_param_token_amount_context token_amount_ctx;
#ifdef HAVE_NFT_SUPPORT
    s_param_nft_context nft_ctx;
#endif
    s_param_datetime_context datetime_ctx;
    s_param_duration_context duration_ctx;
    s_param_unit_context unit_ctx;
#ifdef HAVE_ENUM_VALUE
    s_param_enum_context enum_ctx;
#endif
#ifdef HAVE_TRUSTED_NAME
    s_param_trusted_name_context trusted_name_ctx;
#endif
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
        case PARAM_TYPE_TOKEN_AMOUNT:
#ifdef HAVE_NFT_SUPPORT
        case PARAM_TYPE_NFT:
#endif
        case PARAM_TYPE_DATETIME:
        case PARAM_TYPE_DURATION:
        case PARAM_TYPE_UNIT:
#ifdef HAVE_ENUM_VALUE
        case PARAM_TYPE_ENUM:
#endif
#ifdef HAVE_TRUSTED_NAME
        case PARAM_TYPE_TRUSTED_NAME:
#endif
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
        case PARAM_TYPE_TOKEN_AMOUNT:
            handler = (f_tlv_data_handler) &handle_param_token_amount_struct;
            param_ctx.token_amount_ctx.param = &context->field->param_token_amount;
            break;
#ifdef HAVE_NFT_SUPPORT
        case PARAM_TYPE_NFT:
            handler = (f_tlv_data_handler) &handle_param_nft_struct;
            param_ctx.nft_ctx.param = &context->field->param_nft;
            break;
#endif
        case PARAM_TYPE_DATETIME:
            handler = (f_tlv_data_handler) &handle_param_datetime_struct;
            param_ctx.datetime_ctx.param = &context->field->param_datetime;
            break;
        case PARAM_TYPE_DURATION:
            handler = (f_tlv_data_handler) &handle_param_duration_struct;
            param_ctx.duration_ctx.param = &context->field->param_duration;
            break;
        case PARAM_TYPE_UNIT:
            handler = (f_tlv_data_handler) &handle_param_unit_struct;
            param_ctx.unit_ctx.param = &context->field->param_unit;
            break;
#ifdef HAVE_ENUM_VALUE
        case PARAM_TYPE_ENUM:
            handler = (f_tlv_data_handler) &handle_param_enum_struct;
            param_ctx.enum_ctx.param = &context->field->param_enum;
            break;
#endif
#ifdef HAVE_TRUSTED_NAME
        case PARAM_TYPE_TRUSTED_NAME:
            handler = (f_tlv_data_handler) &handle_param_trusted_name_struct;
            param_ctx.trusted_name_ctx.param = &context->field->param_trusted_name;
            break;
#endif
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
        case PARAM_TYPE_TOKEN_AMOUNT:
            ret = format_param_token_amount(&field->param_token_amount, field->name);
            break;
#ifdef HAVE_NFT_SUPPORT
        case PARAM_TYPE_NFT:
            ret = format_param_nft(&field->param_nft, field->name);
            break;
#endif
        case PARAM_TYPE_DATETIME:
            ret = format_param_datetime(&field->param_datetime, field->name);
            break;
        case PARAM_TYPE_DURATION:
            ret = format_param_duration(&field->param_duration, field->name);
            break;
        case PARAM_TYPE_UNIT:
            ret = format_param_unit(&field->param_unit, field->name);
            break;
#ifdef HAVE_ENUM_VALUE
        case PARAM_TYPE_ENUM:
            ret = format_param_enum(&field->param_enum, field->name);
            break;
#endif
#ifdef HAVE_TRUSTED_NAME
        case PARAM_TYPE_TRUSTED_NAME:
            ret = format_param_trusted_name(&field->param_trusted_name, field->name);
            break;
#endif
        default:
            ret = false;
    }
    return ret;
}

#endif  // HAVE_GENERIC_TX_PARSER
