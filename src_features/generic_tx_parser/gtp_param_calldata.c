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

    (void) name;
    if ((ret = value_get(&param->calldata, &calldatas))) {
        if ((ret = value_get(&param->contract_addr, &contract_addrs)) && (contract_addrs.size == calldatas.size)) {
            if (!param->has_chain_id || ((ret = value_get(&param->chain_id, &chain_ids)) && (chain_ids.size == calldatas.size))) {
                if (!param->has_selector || ((ret = value_get(&param->selector, &selectors)) && (selectors.size == calldatas.size))) {
                    for (int i = 0; i < calldatas.size; ++i) {
                        if (calldatas.value[i].length > 0) {
                            if (get_tx_ctx_count() == 1) {
                                ret = false;
                                break;
                            }
                            uint8_t contract_addr[ADDRESS_LENGTH];
                            uint8_t selector[CALLDATA_SELECTOR_SIZE];
                            const uint8_t *calldata_buf;
                            size_t calldata_length;
                            uint64_t chain_id;
                            uint8_t chain_id_buf[sizeof(chain_id)];
                            s_calldata *calldata;

                            if (param->has_chain_id) {
                                buf_shrink_expand(chain_ids.value[i].ptr,
                                                  chain_ids.value[i].length,
                                                  chain_id_buf,
                                                  sizeof(chain_id_buf));
                                chain_id = read_u64_be(chain_id_buf, 0);
                            } else {
                                const s_tx_info *tx_info = get_current_tx_info();
                                if (tx_info == NULL) {
                                    ret = false;
                                    break;
                                }
                                chain_id = tx_info->chain_id;
                            }

                            if (param->has_selector) {
                                buf_shrink_expand(selectors.value[i].ptr, selectors.value[i].length, selector, sizeof(selector));
                                calldata_buf = calldatas.value[i].ptr;
                                calldata_length = calldatas.value[i].length;
                            } else {
                                if (calldatas.value[i].length < CALLDATA_SELECTOR_SIZE) {
                                    ret = false;
                                    break;
                                }
                                memcpy(selector, calldatas.value[i].ptr, CALLDATA_SELECTOR_SIZE);
                                calldata_buf = calldatas.value[i].ptr + CALLDATA_SELECTOR_SIZE;
                                calldata_length = calldatas.value[i].length - CALLDATA_SELECTOR_SIZE;
                            }

                            buf_shrink_expand(contract_addrs.value[i].ptr,
                                              contract_addrs.value[i].length,
                                              contract_addr,
                                              sizeof(contract_addr));
                            if (!find_matching_tx_ctx(contract_addr, selector, &chain_id)) {
                                ret = false;
                                break;
                            }
                            if (((calldata = calldata_init(calldata_length, selector)) == NULL) ||
                                 !calldata_append(calldata, calldata_buf, calldata_length)) {
                                ret = false;
                                break;
                            }
                            if (!set_calldata_into_tx_ctx(calldata)) {
                                delete_calldata(calldata);
                                ret = false;
                                break;
                            }
                            const s_field_list_node *field;
                            for (field = get_fields_list();
                                 field != NULL;
                                 field = (const s_field_list_node*) ((s_flist_node *) field)->next) {
                                if (!format_field(&field->field)) break;
                            }
                            if (field != NULL) {
                                ret = false;
                                break;
                            }
                            tx_ctx_move_to_parent();
                        }
                    }
                }
            }
        }
    }
    value_cleanup(&param->calldata, &calldatas);
    value_cleanup(&param->contract_addr, &contract_addrs);
    value_cleanup(&param->chain_id, &chain_ids);
    value_cleanup(&param->selector, &selectors);
    return ret;
}
