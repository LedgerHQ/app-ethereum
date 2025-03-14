#include <string.h>
#include "gtp_param_token.h"
#include "shared_context.h"
#include "utils.h"
#include "manage_asset_info.h"
#include "network.h"
#include "gtp_field_table.h"

enum {
    TAG_VERSION = 0x00,
    TAG_ADDRESS = 0x01,
    TAG_NATIVE_CURRENCY = 0x02,
};

static bool handle_version(const s_tlv_data *data, s_param_token_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_address(const s_tlv_data *data, s_param_token_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->address;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_native_currency(const s_tlv_data *data, s_param_token_context *context) {
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

bool handle_param_token_struct(const s_tlv_data *data, s_param_token_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_ADDRESS:
            ret = handle_address(data, context);
            break;
        case TAG_NATIVE_CURRENCY:
            ret = handle_native_currency(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

static bool match_native(const uint8_t *addr, const s_param_token *param) {
    for (int i = 0; i < param->native_addr_count; ++i) {
        if (memcmp(addr, param->native_addrs[i], ADDRESS_LENGTH) == 0) {
            return true;
        }
    }
    return false;
}

bool format_param_token(const s_param_token *param, const char *name) {
    bool ret;
    s_parsed_value_collection collec = {0};
    uint8_t addr[ADDRESS_LENGTH];
    const tokenDefinition_t *token_def;
    uint64_t chain_id;
    const char *ticker = NULL;

    chain_id = get_tx_chain_id();
    if ((ret = value_get(&param->address, &collec))) {
        for (int i = 0; i < collec.size; ++i) {
            buf_shrink_expand(collec.value[i].ptr, collec.value[i].length, addr, sizeof(addr));
            if (match_native(addr, param)) {
                ticker = get_displayable_ticker(&chain_id, chainConfig);
            } else if ((token_def = (const tokenDefinition_t *) get_asset_info_by_addr(addr))) {
                ticker = token_def->ticker;
            }
            if (ticker == NULL) {
                ret = false;
                break;
            }
            if (!(ret = add_to_field_table(PARAM_TYPE_TOKEN, name, ticker))) {
                break;
            }
        }
    }
    value_cleanup(&param->address, &collec);
    return ret;
}
