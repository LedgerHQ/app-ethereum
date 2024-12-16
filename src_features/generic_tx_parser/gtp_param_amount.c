#ifdef HAVE_GENERIC_TX_PARSER

#include "gtp_param_amount.h"
#include "network.h"
#include "common_utils.h"
#include "gtp_field_table.h"
#include "shared_context.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
};

static bool handle_version(const s_tlv_data *data, s_param_amount_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_amount_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_amount_struct(const s_tlv_data *data, s_param_amount_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
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

bool format_param_amount(const s_param_amount *param, const char *name) {
    uint64_t chain_id;
    const char *ticker;
    s_parsed_value_collection collec;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);

    if (!value_get(&param->value, &collec)) {
        return false;
    }
    chain_id = get_tx_chain_id();
    ticker = get_displayable_ticker(&chain_id, chainConfig);
    for (int i = 0; i < collec.size; ++i) {
        if (!amountToString(collec.value[i].ptr,
                            collec.value[i].length,
                            WEI_TO_ETHER,
                            ticker,
                            buf,
                            buf_size)) {
            return false;
        }
        if (!add_to_field_table(PARAM_TYPE_AMOUNT, name, buf)) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_GENERIC_TX_PARSER
