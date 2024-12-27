#ifdef HAVE_GENERIC_TX_PARSER
#ifdef HAVE_TRUSTED_NAME

#include "gtp_param_trusted_name.h"
#include "network.h"
#include "trusted_name.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
    TAG_TYPES = 0x02,
    TAG_SOURCES = 0x03,
};

static bool handle_version(const s_tlv_data *data, s_param_trusted_name_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_trusted_name_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_types(const s_tlv_data *data, s_param_trusted_name_context *context) {
    if (data->length > sizeof(context->param->types)) {
        return false;
    }
    memcpy(context->param->types, data->value, data->length);
    context->param->type_count = data->length;
    return true;
}

static bool handle_sources(const s_tlv_data *data, s_param_trusted_name_context *context) {
    if (data->length > sizeof(context->param->sources)) {
        return false;
    }
    memcpy(context->param->sources, data->value, data->length);
    context->param->source_count = data->length;
    return true;
}

bool handle_param_trusted_name_struct(const s_tlv_data *data,
                                      s_param_trusted_name_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        case TAG_TYPES:
            ret = handle_types(data, context);
            break;
        case TAG_SOURCES:
            ret = handle_sources(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool format_param_trusted_name(const s_param_trusted_name *param, const char *name) {
    s_parsed_value_collection values;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint64_t chain_id;
    uint8_t addr[ADDRESS_LENGTH] = {0};
    const char *tname;
    e_param_type param_type;

    if (!value_get(&param->value, &values)) {
        return false;
    }
    chain_id = get_tx_chain_id();
    for (int i = 0; i < values.size; ++i) {
        buf_shrink_expand(values.value[i].ptr, values.value[i].length, addr, sizeof(addr));
        if ((tname = get_trusted_name(param->type_count,
                                      param->types,
                                      param->source_count,
                                      param->sources,
                                      &chain_id,
                                      addr)) != NULL) {
            strlcpy(buf, tname, buf_size);
            param_type = PARAM_TYPE_TRUSTED_NAME;
        } else {
            getEthDisplayableAddress(addr, buf, buf_size, chainConfig->chainId);
            param_type = PARAM_TYPE_RAW;
        }
        if (!add_to_field_table(param_type, name, buf)) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_TRUSTED_NAME
#endif  // HAVE_GENERIC_TX_PARSER
