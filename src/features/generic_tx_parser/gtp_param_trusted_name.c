#include "gtp_param_trusted_name.h"
#include "tlv_library.h"
#include "gtp_field.h"
#include "network.h"
#include "trusted_name.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"
#include "getPublicKey.h"
#include "apdu_constants.h"
#include "tx_ctx.h"
#include "tlv_apdu.h"

#define PARAM_TRUSTED_NAME_TAGS(X)                           \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG) \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)     \
    X(0x02, TAG_TYPES, handle_types, ENFORCE_UNIQUE_TAG)     \
    X(0x03, TAG_SOURCES, handle_sources, ENFORCE_UNIQUE_TAG) \
    X(0x04, TAG_SENDER_ADDR, handle_sender_addr, ALLOW_MULTIPLE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_trusted_name_context *context) {
    return tlv_get_uint8(data, &context->param->version, 0, UINT8_MAX);
}

static bool handle_value(const tlv_data_t *data, s_param_trusted_name_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return handle_value_struct(&data->value, &ctx);
}

static bool handle_types(const tlv_data_t *data, s_param_trusted_name_context *context) {
    if (data->value.size > sizeof(context->param->types)) {
        return false;
    }
    memcpy(context->param->types, data->value.ptr, data->value.size);
    context->param->type_count = data->value.size;
    return true;
}

static bool handle_sources(const tlv_data_t *data, s_param_trusted_name_context *context) {
    if (data->value.size > sizeof(context->param->sources)) {
        return false;
    }
    memcpy(context->param->sources, data->value.ptr, data->value.size);
    context->param->source_count = data->value.size;
    return true;
}

static bool handle_sender_addr(const tlv_data_t *data, s_param_trusted_name_context *context) {
    if ((data->value.size > sizeof(context->param->sender_addr)) ||
        (context->param->sender_addr_count == ARRAYLEN(context->param->sender_addr))) {
        return false;
    }
    buf_shrink_expand(data->value.ptr,
                      data->value.size,
                      context->param->sender_addr[context->param->sender_addr_count],
                      sizeof(context->param->sender_addr[context->param->sender_addr_count]));
    context->param->sender_addr_count += 1;
    return true;
}

DEFINE_TLV_PARSER(PARAM_TRUSTED_NAME_TAGS, NULL, param_trusted_name_tlv_parser)

bool handle_param_trusted_name_struct(const buffer_t *buf, s_param_trusted_name_context *context) {
    TLV_reception_t received_tags;
    return param_trusted_name_tlv_parser(buf, context, &received_tags);
}

/**
 * @brief Check if an address matches any of the field's constraints
 *
 * @param field Field containing the constraints to check
 * @param values Parsed value collection
 * @param value_index Index of the current value being processed
 * @param addr Address to check against constraints
 * @return true if address matches a constraint, false otherwise
 */
static bool check_address_constraint(const struct s_field *field,
                                     const s_parsed_value_collection *values,
                                     int value_index,
                                     const uint8_t *addr) {
    uint8_t constraint[ADDRESS_LENGTH] = {0};

    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        if (c_node->size > values->value[value_index].length) {
            PRINTF("Warning: TRUSTED_NAME ADDR constraint wrong size!\n");
            continue;
        }
        memset(constraint, 0, sizeof(constraint));
        buf_shrink_expand(c_node->value, c_node->size, constraint, sizeof(constraint));
        if (memcmp(addr, constraint, sizeof(constraint)) == 0) {
            return true;
        }
    }
    return false;
}

bool format_param_trusted_name(const struct s_field *field) {
    bool ret = false;
    s_parsed_value_collection values = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint64_t chain_id;
    uint8_t addr[ADDRESS_LENGTH] = {0};
    const s_trusted_name *tname = NULL;
    e_param_type param_type;
    bool to_be_displayed = true;

    ret = value_get(&field->param_trusted_name.value, &values);
    if (ret) {
        chain_id = get_current_tx_chain_id();
        if (chain_id == 0) {
            ret = false;
            goto cleanup;
        }
        for (int i = 0; i < values.size; ++i) {
            buf_shrink_expand(values.value[i].ptr, values.value[i].length, addr, sizeof(addr));
            // replace by wallet addr if a match is found
            for (uint8_t idx = 0; idx < field->param_trusted_name.sender_addr_count; ++idx) {
                if (memcmp(addr, field->param_trusted_name.sender_addr[idx], ADDRESS_LENGTH) == 0) {
                    if (get_public_key(addr, sizeof(addr)) != SWO_SUCCESS) {
                        ret = false;
                    }
                    break;
                }
            }
            if (!ret) break;
            if ((tname = get_trusted_name(field->param_trusted_name.type_count,
                                          field->param_trusted_name.types,
                                          field->param_trusted_name.source_count,
                                          field->param_trusted_name.sources,
                                          &chain_id,
                                          addr)) != NULL) {
                strlcpy(buf, tname->name, buf_size);
                param_type = PARAM_TYPE_TRUSTED_NAME;
            } else {
                getEthDisplayableAddress(addr, buf, buf_size, chainConfig->chainId);
                param_type = PARAM_TYPE_RAW;
            }

            // Handle visibility constraints
            to_be_displayed = false;
            switch (field->visibility) {
                case PARAM_VISIBILITY_MUST_BE:
                    // Field is not displayed but must match one of the constraint values,
                    // otherwise tx is rejected
                    ret = check_address_constraint(field, &values, i, addr);
                    if (!ret) {
                        PRINTF("Error: TRUSTED_NAME does not match any MUST_BE constraint!\n");
                        // Reject the TX
                        goto cleanup;
                    }
                    break;
                case PARAM_VISIBILITY_IF_NOT_IN:
                    // Field is displayed only if value is NOT in the constraint list
                    ret = check_address_constraint(field, &values, i, addr);
                    if (ret) {
                        PRINTF("Warning: TRUSTED_NAME does match a IF_NOT_IN constraint!\n");
                        // Skip displaying the field
                        goto cleanup;
                    }
                    to_be_displayed = true;
                    break;
                default:  // PARAM_VISIBILITY_ALWAYS
                    to_be_displayed = true;
                    break;
            }

            if (to_be_displayed) {
                if (!(ret = add_to_field_table(param_type, field->name, buf, tname))) {
                    break;
                }
            }
        }
    }

cleanup:
    value_cleanup(&field->param_trusted_name.value, &values);
    return ret;
}
