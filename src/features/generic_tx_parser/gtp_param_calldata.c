#include "gtp_param_calldata.h"
#include "network.h"
#include "common_utils.h"
#include "gtp_field_table.h"
#include "shared_context.h"
#include "gtp_tx_info.h"
#include "utils.h"
#include "read.h"
#include "tx_ctx.h"

enum {
    TAG_VERSION = 0x00,
    TAG_VALUE = 0x01,
    TAG_CALLEE = 0x02,
    TAG_CHAIN_ID = 0x03,
    TAG_SELECTOR = 0x04,
    TAG_AMOUNT = 0x05,
    TAG_SPENDER = 0x06,
};

static bool handle_version(const s_tlv_data *data, s_param_calldata_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];
    return true;
}

static bool handle_value(const s_tlv_data *data, s_param_calldata_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->calldata;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_callee(const s_tlv_data *data, s_param_calldata_context *context) {
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

static bool handle_amount(const s_tlv_data *data, s_param_calldata_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->amount;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    context->param->has_amount = true;
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

static bool handle_spender(const s_tlv_data *data, s_param_calldata_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->spender;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    context->param->has_spender = true;
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

bool handle_param_calldata_struct(const s_tlv_data *data, s_param_calldata_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        case TAG_CALLEE:
            ret = handle_callee(data, context);
            break;
        case TAG_CHAIN_ID:
            ret = handle_chain_id(data, context);
            break;
        case TAG_SELECTOR:
            ret = handle_selector(data, context);
            break;
        case TAG_AMOUNT:
            ret = handle_amount(data, context);
            break;
        case TAG_SPENDER:
            ret = handle_spender(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

static bool process_nested_calldata(const s_param_calldata *param,
                                    const s_parsed_value *calldata,
                                    const s_parsed_value *contract_addr,
                                    const s_parsed_value *chain_id,
                                    const s_parsed_value *selector,
                                    const s_parsed_value *amount,
                                    const s_parsed_value *spender) {
    uint8_t addr_buf[ADDRESS_LENGTH];
    uint8_t selector_buf[CALLDATA_SELECTOR_SIZE];
    const uint8_t *calldata_buf;
    size_t calldata_length;
    s_calldata *new_calldata = NULL;
    uint8_t amount_buf[INT256_LENGTH];
    uint8_t from_buf[ADDRESS_LENGTH];
    uint64_t chain_id_value;
    uint8_t chain_id_buf[sizeof(chain_id_value)];

    if (param->has_chain_id) {
        buf_shrink_expand(chain_id->ptr, chain_id->length, chain_id_buf, sizeof(chain_id_buf));
        chain_id_value = read_u64_be(chain_id_buf, 0);
    }

    if (calldata->length > 0) {
        if (param->has_selector) {
            buf_shrink_expand(selector->ptr, selector->length, selector_buf, sizeof(selector_buf));
            calldata_buf = calldata->ptr;
            calldata_length = calldata->length;
        } else {
            if (calldata->length < CALLDATA_SELECTOR_SIZE) {
                return false;
            }
            memcpy(selector_buf, calldata->ptr, CALLDATA_SELECTOR_SIZE);
            calldata_buf = calldata->ptr + CALLDATA_SELECTOR_SIZE;
            calldata_length = calldata->length - CALLDATA_SELECTOR_SIZE;
        }

        if (((new_calldata = calldata_init(calldata_length, selector_buf)) == NULL) ||
            !calldata_append(new_calldata, calldata_buf, calldata_length)) {
            return false;
        }
    }

    buf_shrink_expand(contract_addr->ptr, contract_addr->length, addr_buf, sizeof(addr_buf));
    if (param->has_amount) {
        buf_shrink_expand(amount->ptr, amount->length, amount_buf, sizeof(amount_buf));
    }
    if (param->has_spender) {
        buf_shrink_expand(spender->ptr, spender->length, from_buf, sizeof(from_buf));
    }

    if (!tx_ctx_init(new_calldata,
                     param->has_spender ? from_buf : NULL,
                     addr_buf,
                     param->has_amount ? amount_buf : NULL,
                     param->has_chain_id ? &chain_id_value : NULL)) {
        if (new_calldata != NULL) {
            calldata_delete(new_calldata);
        }
        return false;
    }
    return true;
}

static bool check_param(const s_param_calldata *param,
                        s_parsed_value_collection *calldatas,
                        s_parsed_value_collection *contract_addrs,
                        s_parsed_value_collection *chain_ids,
                        s_parsed_value_collection *selectors,
                        s_parsed_value_collection *amounts,
                        s_parsed_value_collection *spenders) {
    if (!value_get(&param->calldata, calldatas)) return false;
    if (!value_get(&param->contract_addr, contract_addrs)) return false;
    if (contract_addrs->size != calldatas->size) return false;

    if (param->has_chain_id) {
        if (!value_get(&param->chain_id, chain_ids)) return false;
        if (chain_ids->size != calldatas->size) return false;
    }
    if (param->has_selector) {
        if (!value_get(&param->selector, selectors)) return false;
        if (selectors->size != calldatas->size) return false;
    }
    if (param->has_amount) {
        if (!value_get(&param->amount, amounts)) return false;
        if (amounts->size != calldatas->size) return false;
    }
    if (param->has_spender) {
        if (!value_get(&param->spender, spenders)) return false;
        if (spenders->size != calldatas->size) return false;
    }
    return true;
}

bool format_param_calldata(const s_param_calldata *param, const char *name) {
    bool ret;
    s_parsed_value_collection calldatas = {0};
    s_parsed_value_collection contract_addrs = {0};
    s_parsed_value_collection chain_ids = {0};
    s_parsed_value_collection selectors = {0};
    s_parsed_value_collection amounts = {0};
    s_parsed_value_collection spenders = {0};

    (void) name;
    if ((ret = check_param(param,
                           &calldatas,
                           &contract_addrs,
                           &chain_ids,
                           &selectors,
                           &amounts,
                           &spenders))) {
        // Set batch size and number of transactions
        if (calldatas.size > 1) {
            txContext.batch_nb_tx += calldatas.size;
        }
        txContext.current_batch_size = calldatas.size;
        for (int i = 0; i < calldatas.size; ++i) {
            if (!process_nested_calldata(param,
                                         &calldatas.value[i],
                                         &contract_addrs.value[i],
                                         &chain_ids.value[i],
                                         &selectors.value[i],
                                         &amounts.value[i],
                                         &spenders.value[i])) {
                ret = false;
                break;
            }
        }
    }
    value_cleanup(&param->calldata, &calldatas);
    value_cleanup(&param->contract_addr, &contract_addrs);
    value_cleanup(&param->chain_id, &chain_ids);
    value_cleanup(&param->selector, &selectors);
    value_cleanup(&param->amount, &amounts);
    value_cleanup(&param->spender, &spenders);
    return ret;
}
