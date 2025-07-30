#include "gtp_param_calldata.h"
#include "network.h"
#include "common_utils.h"
#include "gtp_field_table.h"
#include "shared_context.h"
#include "gtp_tx_info.h"

enum {
    TAG_VERSION = 0x00,
    TAG_CALLDATA = 0x01,
    TAG_CONTRACT_ADDR = 0x02,
    TAG_CHAIN_ID = 0x03,
    TAG_SELECTOR = 0x04,
};

static bool handle_version(const s_tlv_data *data, s_param_calldata_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_calldata(const s_tlv_data *data, s_param_calldata_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->calldata;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_contract_addr(const s_tlv_data *data, s_param_calldata_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->contract_addr;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_chain_id(const s_tlv_data *data, s_param_calldata_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->chain_id;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    context->param->has_chain_id = true;
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_selector(const s_tlv_data *data, s_param_calldata_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->selector;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    context->param->has_selector = true;
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_calldata_struct(const s_tlv_data *data, s_param_calldata_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_CALLDATA:
            ret = handle_calldata(data, context);
            break;
        case TAG_CONTRACT_ADDR:
            ret = handle_contract_addr(data, context);
            break;
        case TAG_CHAIN_ID:
            ret = handle_chain_id(data, context);
            break;
        case TAG_SELECTOR:
            ret = handle_selector(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

bool format_param_calldata(const s_param_calldata *param, const char *name) {
    bool ret = true;
    s_parsed_value_collection calldatas = {0};
    s_parsed_value_collection contract_addrs = {0};
    s_parsed_value_collection chain_ids = {0};
    s_parsed_value_collection selectors = {0};

    (void) param;
    (void) name;
    if (get_tx_ctx_count() == 1) {
        return false;
    }
    if ((ret = value_get(&param->calldata, &calldatas))) {
        if ((ret = value_get(&param->contract_addr, &contract_addrs)) && (contract_addrs.size == calldatas.size)) {
            if (!param->has_chain_id || ((ret = value_get(&param->chain_id, &chain_ids)) && (chain_ids.size == calldatas.size))) {
                if (!param->has_selector || ((ret = value_get(&param->selector, &selectors)) && (selectors.size == calldatas.size))) {
                    for (int i = 0; i < calldatas.size; ++i) {
                        PRINTF("calldata -> 0x%.*h\n", calldatas.value[i].length, &calldatas.value[i].ptr[calldatas.value[i].offset]);
                        // TODO: handle given selector
                        calldata_init(calldatas.value[i].length - CALLDATA_SELECTOR_SIZE,
                                      &calldatas.value[i].ptr[calldatas.value[i].offset]);
                        calldata_append(&calldatas.value[i].ptr[calldatas.value[i].offset + CALLDATA_SELECTOR_SIZE],
                                        calldatas.value[i].length - CALLDATA_SELECTOR_SIZE);
                        // TODO: find matching tx info
                        // TODO: format saved fields
                        calldata_pop();
                    }
                }
            }
        }
    }
    value_cleanup(&param->calldata, &calldatas);
    value_cleanup(&param->contract_addr, &contract_addrs);
    value_cleanup(&param->chain_id, &chain_ids);
    value_cleanup(&param->selector, &selectors);
    //tx_info_pop();
    return ret;
}
