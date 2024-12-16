#ifdef HAVE_GENERIC_TX_PARSER

#include "gtp_param_unit.h"
#include "gtp_field_table.h"
#include "shared_context.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
    TAG_BASE = 0x02,
    TAG_DECIMALS = 0x03,
    TAG_PREFIX = 0x04,
};

static bool handle_version(const s_tlv_data *data, s_param_unit_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_unit_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_base(const s_tlv_data *data, s_param_unit_context *context) {
    if (data->length >= sizeof(context->param->base)) {
        return false;
    }
    memcpy(context->param->base, data->value, data->length);
    context->param->base[data->length] = '\0';
    return true;
}

static bool handle_decimals(const s_tlv_data *data, s_param_unit_context *context) {
    if (data->length != sizeof(context->param->decimals)) {
        return false;
    }
    context->param->decimals = data->value[0];
    return true;
}

static bool handle_prefix(const s_tlv_data *data, s_param_unit_context *context) {
    if (data->length != sizeof(bool)) {
        return false;
    }
    (void) context;
    // unused for now
    return true;
}

bool handle_param_unit_struct(const s_tlv_data *data, s_param_unit_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        case TAG_BASE:
            ret = handle_base(data, context);
            break;
        case TAG_DECIMALS:
            ret = handle_decimals(data, context);
            break;
        case TAG_PREFIX:
            ret = handle_prefix(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool format_param_unit(const s_param_unit *param, const char *name) {
    s_parsed_value_collection collec;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    char tmp[80];
    size_t off;

    if (!value_get(&param->value, &collec)) {
        return false;
    }
    for (int i = 0; i < collec.size; ++i) {
        if (!uint256_to_decimal(collec.value[i].ptr, collec.value[i].length, tmp, sizeof(tmp))) {
            return false;
        }
        if (!adjustDecimals(tmp, strnlen(tmp, sizeof(tmp)), buf, buf_size, param->decimals)) {
            return false;
        }
        if (param->base[0] == '\0') {
            return false;
        }
        off = strlen(buf);
        snprintf(&buf[off], buf_size - off, " %s", param->base);

        if (!add_to_field_table(PARAM_TYPE_UNIT, name, buf)) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_GENERIC_TX_PARSER
