#include "gtp_param_amount.h"
#include "network.h"
#include "common_utils.h"
#include "gtp_field_table.h"
#include "shared_context.h"
#include "tx_ctx.h"
#include "tlv_library.h"

#define PARAM_AMOUNT_TAGS(X)                                 \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_amount_context *context) {
    if (data->value.size != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value.ptr[0];
    return true;
}

static bool handle_value(const tlv_data_t *data, s_param_amount_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

DEFINE_TLV_PARSER(PARAM_AMOUNT_TAGS, NULL, param_amount_tlv_parser)

bool handle_param_amount_struct(const buffer_t *buf, s_param_amount_context *context) {
    TLV_reception_t received_tags;
    return param_amount_tlv_parser(buf, context, &received_tags);
}

bool format_param_amount(const s_param_amount *param, const char *name) {
    bool ret;
    uint64_t chain_id;
    const char *ticker;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);

    if ((ret = value_get(&param->value, &collec))) {
        if (get_current_tx_info() == NULL) return false;
        chain_id = get_current_tx_info()->chain_id;
        ticker = get_displayable_ticker(&chain_id, chainConfig, true);
        for (int i = 0; i < collec.size; ++i) {
            if (!(ret = amountToString(collec.value[i].ptr,
                                       collec.value[i].length,
                                       WEI_TO_ETHER,
                                       ticker,
                                       buf,
                                       buf_size))) {
                break;
            }
            if (!(ret = add_to_field_table(PARAM_TYPE_AMOUNT, name, buf, NULL))) {
                break;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
