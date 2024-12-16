#ifdef HAVE_GENERIC_TX_PARSER

#include <string.h>
#include "gtp_param_token_amount.h"
#include "network.h"
#include "utils.h"
#include "gtp_field_table.h"
#include "manage_asset_info.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
    TAG_TOKEN = 0x02,
    TAG_NATIVE_CURRENCY = 0x03,
    TAG_THRESHOLD = 0x04,
    TAG_ABOVE_THRESHOLD_MSG = 0x05,
};

static bool handle_version(const s_tlv_data *data, s_param_token_amount_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_token_amount_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_token(const s_tlv_data *data, s_param_token_amount_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->token;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    context->param->has_token = true;
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_native_currency(const s_tlv_data *data, s_param_token_amount_context *context) {
    if (data->length > ADDRESS_LENGTH) {
        return false;
    }
    if (context->param->native_addr_count == MAX_NATIVE_ADDRS) {
        return false;
    }
    memcpy(&context->param
                ->native_addrs[context->param->native_addr_count][ADDRESS_LENGTH - data->length],
           data->value,
           data->length);
    context->param->native_addr_count += 1;
    return true;
}

static bool handle_threshold(const s_tlv_data *data, s_param_token_amount_context *context) {
    if (data->length > sizeof(uint256_t)) {
        return false;
    }
    convertUint256BE(data->value, data->length, &context->param->threshold);
    return true;
}

static bool handle_above_threshold_msg(const s_tlv_data *data,
                                       s_param_token_amount_context *context) {
    if (data->length >= sizeof(context->param->above_threshold_msg)) {
        return false;
    }
    memcpy(context->param->above_threshold_msg, data->value, data->length);
    context->param->above_threshold_msg[data->length] = '\0';
    return true;
}

bool handle_param_token_amount_struct(const s_tlv_data *data,
                                      s_param_token_amount_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        case TAG_TOKEN:
            ret = handle_token(data, context);
            break;
        case TAG_NATIVE_CURRENCY:
            ret = handle_native_currency(data, context);
            break;
        case TAG_THRESHOLD:
            ret = handle_threshold(data, context);
            break;
        case TAG_ABOVE_THRESHOLD_MSG:
            ret = handle_above_threshold_msg(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

static bool match_native(const uint8_t *addr, const s_param_token_amount *param) {
    for (int i = 0; i < param->native_addr_count; ++i) {
        if (memcmp(addr, param->native_addrs[i], ADDRESS_LENGTH) == 0) {
            return true;
        }
    }
    return false;
}

bool format_param_token_amount(const s_param_token_amount *param, const char *name) {
    s_parsed_value_collection collec_value;
    s_parsed_value_collection collec_token;
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint8_t addr_buf[ADDRESS_LENGTH];

    if (!value_get(&param->value, &collec_value)) {
        return false;
    }
    if (param->has_token) {
        if (!value_get(&param->token, &collec_token)) {
            return false;
        }
        if (collec_value.size != collec_token.size) {
            PRINTF("Error: mismatch between counts of value & token!\n");
            return false;
        }
    } else {
        explicit_bzero(&collec_token, sizeof(collec_token));
    }
    uint64_t chain_id = get_tx_chain_id();
    const char *ticker = g_unknown_ticker;
    uint8_t decimals = 0;
    for (int i = 0; i < collec_value.size; ++i) {
        if (param->has_token) {
            buf_shrink_expand(collec_token.value[i].ptr,
                              collec_token.value[i].length,
                              addr_buf,
                              sizeof(addr_buf));
            if (match_native(addr_buf, param)) {
                ticker = get_displayable_ticker(&chain_id, chainConfig);
                decimals = WEI_TO_ETHER;
            } else {
                const tokenDefinition_t *token;
                if ((token = (const tokenDefinition_t *) get_asset_info_by_addr(addr_buf)) !=
                    NULL) {
                    ticker = token->ticker;
                    decimals = token->decimals;
                }
            }
        }
        uint256_t zero256 = {0};
        uint256_t val256;

        convertUint256BE(collec_value.value[i].ptr, collec_value.value[i].length, &val256);
        if (!equal256(&param->threshold, &zero256) && gte256(&val256, &param->threshold)) {
            if (param->above_threshold_msg[0] != '\0') {
                snprintf(buf, buf_size, "%s %s", param->above_threshold_msg, ticker);
            } else {
                snprintf(buf, buf_size, "Unlimited %s", ticker);
            }
        } else {
            if (!amountToString(collec_value.value[i].ptr,
                                collec_value.value[i].length,
                                decimals,
                                ticker,
                                buf,
                                buf_size)) {
                return false;
            }
        }
        if (!add_to_field_table(PARAM_TYPE_AMOUNT, name, buf)) {
            return false;
        }
    }
    return true;
}

#endif  // HAVE_GENERIC_TX_PARSER
