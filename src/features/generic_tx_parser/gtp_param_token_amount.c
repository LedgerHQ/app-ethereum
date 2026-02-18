#include <string.h>
#include "gtp_param_token_amount.h"
#include "tlv_library.h"
#include "network.h"
#include "utils.h"
#include "gtp_field_table.h"
#include "manage_asset_info.h"
#include "tx_ctx.h"
#include "tlv_apdu.h"

#define PARAM_TOKEN_AMOUNT_TAGS(X)                                           \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)                 \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)                     \
    X(0x02, TAG_TOKEN, handle_token, ALLOW_MULTIPLE_TAG)                     \
    X(0x03, TAG_NATIVE_CURRENCY, handle_native_currency, ALLOW_MULTIPLE_TAG) \
    X(0x04, TAG_THRESHOLD, handle_threshold, ENFORCE_UNIQUE_TAG)             \
    X(0x05, TAG_ABOVE_THRESHOLD_MSG, handle_above_threshold_msg, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_token_amount_context *context) {
    return tlv_get_uint8(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_value(const tlv_data_t *data, s_param_token_amount_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

static bool handle_token(const tlv_data_t *data, s_param_token_amount_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->token;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    context->param->has_token = true;
    return handle_value_struct(&data->value, &ctx);
}

static bool handle_native_currency(const tlv_data_t *data, s_param_token_amount_context *context) {
    if (data->value.size > ADDRESS_LENGTH) {
        return false;
    }
    if (context->param->native_addr_count == MAX_NATIVE_ADDRS) {
        return false;
    }
    memcpy(&context->param->native_addrs[context->param->native_addr_count]
                                        [ADDRESS_LENGTH - data->value.size],
           data->value.ptr,
           data->value.size);
    context->param->native_addr_count += 1;
    return true;
}

static bool handle_threshold(const tlv_data_t *data, s_param_token_amount_context *context) {
    if (data->value.size > sizeof(uint256_t)) {
        return false;
    }
    convertUint256BE(data->value.ptr, data->value.size, &context->param->threshold);
    return true;
}

static bool handle_above_threshold_msg(const tlv_data_t *data,
                                       s_param_token_amount_context *context) {
    if (data->value.size >= sizeof(context->param->above_threshold_msg)) {
        return false;
    }
    memcpy(context->param->above_threshold_msg, data->value.ptr, data->value.size);
    context->param->above_threshold_msg[data->value.size] = '\0';
    return true;
}

DEFINE_TLV_PARSER(PARAM_TOKEN_AMOUNT_TAGS, NULL, param_token_amount_tlv_parser)

bool handle_param_token_amount_struct(const buffer_t *buf, s_param_token_amount_context *context) {
    TLV_reception_t received_tags;
    return param_token_amount_tlv_parser(buf, context, &received_tags);
}

static bool match_native(const uint8_t *addr, const s_param_token_amount *param) {
    for (int i = 0; i < param->native_addr_count; ++i) {
        if (memcmp(addr, param->native_addrs[i], ADDRESS_LENGTH) == 0) {
            return true;
        }
    }
    return false;
}

static bool process_token_amount(const s_param_token_amount *param,
                                 const char *name,
                                 s_parsed_value *value,
                                 s_parsed_value *token) {
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint8_t addr_buf[ADDRESS_LENGTH];
    uint256_t zero256 = {0};
    uint256_t val256;
    const tokenDefinition_t *token_def = NULL;
    uint64_t chain_id;
    const char *ticker = g_unknown_ticker;
    uint8_t decimals = 0;

    if (get_current_tx_info() == NULL) return false;
    chain_id = get_current_tx_info()->chain_id;
    if (param->has_token) {
        buf_shrink_expand(token->ptr, token->length, addr_buf, sizeof(addr_buf));
        if (match_native(addr_buf, param)) {
            ticker = get_displayable_ticker(&chain_id, chainConfig, true);
            decimals = WEI_TO_ETHER;
        } else {
            if ((token_def = (const tokenDefinition_t *) get_asset_info_by_addr(addr_buf)) !=
                NULL) {
                ticker = token_def->ticker;
                decimals = token_def->decimals;
            }
        }
    }

    convertUint256BE(value->ptr, value->length, &val256);
    if (!equal256(&param->threshold, &zero256) && gte256(&val256, &param->threshold)) {
        if (param->above_threshold_msg[0] != '\0') {
            snprintf(buf, buf_size, "%s %s", param->above_threshold_msg, ticker);
        } else {
            snprintf(buf, buf_size, "Unlimited %s", ticker);
        }
    } else {
        if (!amountToString(value->ptr, value->length, decimals, ticker, buf, buf_size)) {
            return false;
        }
    }
    if (!add_to_field_table(token_def ? PARAM_TYPE_TOKEN_AMOUNT : PARAM_TYPE_AMOUNT,
                            name,
                            buf,
                            token_def)) {
        return false;
    }
    return true;
}

bool format_param_token_amount(const s_param_token_amount *param, const char *name) {
    bool ret;
    s_parsed_value_collection collec_value = {0};
    s_parsed_value_collection collec_token = {0};

    if ((ret = value_get(&param->value, &collec_value))) {
        if (param->has_token) {
            if ((ret = value_get(&param->token, &collec_token))) {
                if (collec_value.size != collec_token.size) {
                    PRINTF("Error: mismatch between counts of value & token!\n");
                    ret = false;
                }
            }
        }
        if (ret) {
            for (int i = 0; i < collec_value.size; ++i) {
                if (!(ret = process_token_amount(param,
                                                 name,
                                                 &collec_value.value[i],
                                                 &collec_token.value[i]))) {
                    break;
                }
            }
        }
    }
    value_cleanup(&param->value, &collec_value);
    value_cleanup(&param->token, &collec_token);
    return ret;
}
