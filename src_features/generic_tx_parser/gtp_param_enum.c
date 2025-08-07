#include "gtp_param_enum.h"
#include "network.h"
#include "enum_value.h"
#include "gtp_field_table.h"
#include "calldata.h"
#include "shared_context.h"

enum {
    TAG_VERSION = 0x00,
    TAG_ID = 0x01,
    TAG_VALUE = 0x02,
};

static bool handle_version(const s_tlv_data *data, s_param_enum_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_id(const s_tlv_data *data, s_param_enum_context *context) {
    if (data->length != sizeof(context->param->id)) {
        return false;
    }
    context->param->id = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_enum_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_enum_struct(const s_tlv_data *data, s_param_enum_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_ID:
            ret = handle_id(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool format_param_enum(const s_param_enum *param, const char *name) {
    bool ret;
    uint64_t chain_id;
    s_parsed_value_collection collec = {0};
    const char *enum_name;
    uint8_t value;
    const uint8_t *selector;

    if ((ret = value_get(&param->value, &collec))) {
        chain_id = get_tx_chain_id();
        for (int i = 0; i < collec.size; ++i) {
            if (collec.value[i].length == 0) {
                ret = false;
                break;
            }
            value = collec.value[i].ptr[collec.value[i].length - 1];
            if ((selector = calldata_get_selector()) == NULL) {
                ret = false;
                break;
            }
            enum_name = get_matching_enum_name(&chain_id,
                                               txContext.content->destination,
                                               selector,
                                               param->id,
                                               value);
            if (!(ret = add_to_field_table(PARAM_TYPE_ENUM, name, enum_name))) {
                break;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
