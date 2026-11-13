#include "gcs_param_amount.h"
#include "network.h"
#include "common_utils.h"
#include "gcs_field_table.h"
#include "shared_context.h"
#include "operation_ctx.h"
#include "tlv_library.h"
#include "tlv_utils.h"

#define PARAM_AMOUNT_STRUCT_VERSION 0x01

#define PARAM_AMOUNT_TAGS(X)                                 \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_amount_context *context) {
    return tlv_get_uint8_range(data,
                               &context->param->version,
                               PARAM_AMOUNT_STRUCT_VERSION,
                               PARAM_AMOUNT_STRUCT_VERSION);
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
    if (!param_amount_tlv_parser(buf, context, &received_tags)) {
        return false;
    }
    // Enforce the sub-structure's mandatory tags: an empty or partial PARAM
    // payload would otherwise parse fine and never appear in the review
    if (!TLV_CHECK_RECEIVED_TAGS(received_tags, TAG_VERSION, TAG_VALUE)) {
        PRINTF("Error: missing mandatory tag(s) in gcs_param_amount\n");
        return false;
    }
    return true;
}

__attribute__((noinline)) bool format_param_amount(const s_param_amount *param, const char *name) {
    bool ret;
    uint64_t chain_id;
    const char *ticker;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);

    if ((ret = value_get(&param->value, &collec))) {
        if (!get_current_operation_chain_id(&chain_id)) {
            ret = false;
            goto end;
        }
        ticker = get_displayable_ticker(&chain_id, g_chain_config, true);
        for (int i = 0; i < collec.size; ++i) {
            if ((collec.value[i].length == 0) || (collec.value[i].length > INT256_LENGTH)) {
                PRINTF("Error: invalid amount length (%u)\n", collec.value[i].length);
                ret = false;
                break;
            }
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
            value_set_shown(&param->value, i);
        }
    }
end:
    value_cleanup(&param->value, &collec);
    return ret;
}
