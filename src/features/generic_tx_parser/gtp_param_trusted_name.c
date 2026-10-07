#include "gtp_param_trusted_name.h"
#include "tlv_library.h"
#include "gtp_field.h"
#include "network.h"
#include "trusted_name.h"
#include "address_name_lookup.h"
#include "gtp_field_table.h"
#include "utils.h"
#include "shared_context.h"
#include "get_public_key.h"
#include "apdu_constants.h"
#include "tx_ctx.h"
#include "tlv_utils.h"

#define PARAM_TRUSTED_NAME_TAGS(X)                                   \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)         \
    X(0x01, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)             \
    X(0x02, TAG_TYPES, handle_types, ENFORCE_UNIQUE_TAG)             \
    X(0x03, TAG_SOURCES, handle_sources, ENFORCE_UNIQUE_TAG)         \
    X(0x04, TAG_SENDER_ADDR, handle_sender_addr, ALLOW_MULTIPLE_TAG) \
    X(0x05, TAG_VALUE_TYPE, handle_value_type, ENFORCE_UNIQUE_TAG)

static bool handle_version(const tlv_data_t *data, s_param_trusted_name_context *context) {
    return tlv_get_uint8_range(data, &context->param->version, 0, UINT8_MAX);
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
    if (data->value.size > 0) {
        if (data->value.ptr == NULL) {
            return false;
        }
        memcpy(context->param->types, data->value.ptr, data->value.size);
    }
    context->param->type_count = data->value.size;
    return true;
}

static bool handle_sources(const tlv_data_t *data, s_param_trusted_name_context *context) {
    if (data->value.size > sizeof(context->param->sources)) {
        return false;
    }
    if (data->value.size > 0) {
        if (data->value.ptr == NULL) {
            return false;
        }
        memcpy(context->param->sources, data->value.ptr, data->value.size);
    }
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

static bool handle_value_type(const tlv_data_t *data, s_param_trusted_name_context *context) {
    uint8_t value_type;

    if (!tlv_get_uint8_range(data, &value_type, TNVT_STANDARD, TNVT_INTEROPERABLE)) {
        return false;
    }
    context->param->value_type = (e_trusted_name_value_type) value_type;
    return true;
}

DEFINE_TLV_PARSER(PARAM_TRUSTED_NAME_TAGS, NULL, param_trusted_name_tlv_parser)

bool handle_param_trusted_name_struct(const buffer_t *buf, s_param_trusted_name_context *context) {
    TLV_reception_t received_tags;
    if (!param_trusted_name_tlv_parser(buf, context, &received_tags)) {
        return false;
    }
    // Enforce the sub-structure's mandatory tags: an empty or partial PARAM
    // payload would otherwise parse fine and never appear in the review
    if (!TLV_CHECK_RECEIVED_TAGS(received_tags, TAG_VERSION, TAG_VALUE)) {
        PRINTF("Error: missing mandatory tag(s) in gtp_param_trusted_name\n");
        return false;
    }
    return true;
}

/**
 * @brief Check if an address matches any of the field's constraints
 */
static bool check_address_constraint(const struct s_field *field,
                                     const s_parsed_value_collection *values,
                                     int value_index,
                                     const uint8_t *addr,
                                     bool is_interoperable) {
    uint8_t constraint[ADDRESS_LENGTH] = {0};
    const uint8_t *raw_value = values->value[value_index].ptr;
    uint16_t raw_length = values->value[value_index].length;

    for (s_field_constraint *c_node = field->constraints; c_node != NULL;
         c_node = (s_field_constraint *) c_node->node.next) {
        if (is_interoperable) {
            // The value is a complete interoperable address, so the address alone does
            // not identify it: the same address on another chain would satisfy a
            // constraint matched on that part only. Compare the whole value instead,
            // which keeps the constraint an opaque byte array like every other
            // formatter's.
            if ((c_node->size == raw_length) &&
                (memcmp(c_node->value, raw_value, raw_length) == 0)) {
                return true;
            }
            continue;
        }
        if (c_node->size > raw_length) {
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

/**
 * @brief Parse an EIP-7930 interoperable address into chain_id and EVM address
 *
 * EIP-7930 binary format for EVM: [chain_id (1-8 bytes, big-endian)][address (20 bytes)]
 *
 * @param buf   Raw bytes of the interoperable address
 * @param len   Length of buf
 * @param chain_id  Output: parsed chain ID
 * @param addr  Output: 20-byte EVM address
 * @return true on success, false if format is invalid
 */
static bool parse_eip7930_address(const uint8_t *buf,
                                  size_t len,
                                  uint64_t *chain_id,
                                  uint8_t addr[ADDRESS_LENGTH]) {
    if (len <= ADDRESS_LENGTH || len > ADDRESS_LENGTH + sizeof(uint64_t)) {
        PRINTF("Error: invalid EIP-7930 address length: %d\n", (int) len);
        return false;
    }
    size_t chain_id_len = len - ADDRESS_LENGTH;
    *chain_id = u64_from_BE(buf, chain_id_len);
    if (*chain_id == 0) {
        PRINTF("Error: EIP-7930 chain_id is zero\n");
        return false;
    }
    memcpy(addr, buf + chain_id_len, ADDRESS_LENGTH);
    return true;
}

/**
 * @brief Append " (NetworkName)" suffix to buf, the chain ID when the network is unknown
 *
 * @return false if the network string cannot be produced or the suffix does not fit
 */
static bool append_network_suffix(char *buf, size_t buf_size, uint64_t chain_id) {
    // large enough for a network name or any decimal uint64 chain ID
    char network_name[MAX(NETWORK_STRING_MAX_SIZE + 1, sizeof("18446744073709551615"))] = {0};

    if (!get_network_as_string_from_chain_id(network_name, sizeof(network_name), chain_id)) {
        return false;
    }
    strlcat(buf, " (", buf_size);
    strlcat(buf, network_name, buf_size);
    return strlcat(buf, ")", buf_size) < buf_size;
}

bool format_param_trusted_name(const struct s_field *field) {
    bool ret = false;
    s_parsed_value_collection values = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);
    uint64_t chain_id = 0;
    uint8_t addr[ADDRESS_LENGTH] = {0};
    e_param_type param_type;
    bool to_be_displayed = true;
    bool is_interoperable = (field->param_trusted_name.value_type == TNVT_INTEROPERABLE);

    ret = value_get(&field->param_trusted_name.value, &values);
    if (!ret) {
        goto cleanup;
    }

    if (!is_interoperable) {
        chain_id = get_current_tx_chain_id();
        if (chain_id == 0) {
            ret = false;
            goto cleanup;
        }
    }

    for (int i = 0; i < values.size; ++i) {
        if (is_interoperable) {
            // EIP-7930: extract chain_id and address from the encoded bytes
            if (!parse_eip7930_address(values.value[i].ptr,
                                       values.value[i].length,
                                       &chain_id,
                                       addr)) {
                ret = false;
                break;
            }
        } else {
            buf_shrink_expand(values.value[i].ptr, values.value[i].length, addr, sizeof(addr));
        }
        // Replace by wallet addr if a match is found in sender_addr list
        for (uint8_t idx = 0; idx < field->param_trusted_name.sender_addr_count; ++idx) {
            if (memcmp(addr, field->param_trusted_name.sender_addr[idx], ADDRESS_LENGTH) == 0) {
                if (get_public_key(addr, sizeof(addr)) != SWO_SUCCESS) {
                    ret = false;
                }
                break;
            }
        }
        if (!ret) break;

        // Address Book > Trusted Name > raw hex; extra_data carries the matched record for the
        // alias detail view
        e_addr_name_source name_source;
        const void *extra_data = NULL;
        if (!get_address_display_name(addr,
                                      chain_id,
                                      field->param_trusted_name.type_count,
                                      field->param_trusted_name.types,
                                      field->param_trusted_name.source_count,
                                      field->param_trusted_name.sources,
                                      buf,
                                      buf_size,
                                      &name_source,
                                      &extra_data)) {
            ret = false;
            break;
        }
        // param_type drives how the field is rendered: ADDRESS_BOOK and TRUSTED_NAME
        // entries carry an alias detail view (tappable name), RAW shows the plain address
        switch (name_source) {
            case ADDR_NAME_FROM_ADDRESS_BOOK:
                param_type = PARAM_TYPE_ADDRESS_BOOK;
                break;
            case ADDR_NAME_FROM_TRUSTED_NAME:
                param_type = PARAM_TYPE_TRUSTED_NAME;
                break;
            default:
                param_type = PARAM_TYPE_RAW;
                break;
        }

        if (is_interoperable && !append_network_suffix(buf, buf_size, chain_id)) {
            ret = false;
            break;
        }

        // Handle visibility constraints
        to_be_displayed = false;
        switch (field->visibility) {
            case PARAM_VISIBILITY_MUST_BE:
                // Field is not displayed but must match one of the constraint values,
                // otherwise tx is rejected
                ret = check_address_constraint(field, &values, i, addr, is_interoperable);
                if (!ret) {
                    PRINTF("Error: TRUSTED_NAME does not match any MUST_BE constraint!\n");
                    // Reject the TX
                    goto cleanup;
                }
                break;
            case PARAM_VISIBILITY_IF_NOT_IN:
                // Field is displayed only if value is NOT in the constraint list
                if (check_address_constraint(field, &values, i, addr, is_interoperable)) {
                    PRINTF("Warning: TRUSTED_NAME does match a IF_NOT_IN constraint!\n");
                    // Skip displaying only this value, continue with remaining values
                    continue;
                }
                to_be_displayed = true;
                break;
            default:  // PARAM_VISIBILITY_ALWAYS
                to_be_displayed = true;
                break;
        }

        if (to_be_displayed) {
            if (!(ret = add_to_field_table(param_type, field->name, buf, extra_data))) {
                break;
            }
        }
    }

cleanup:
    value_cleanup(&field->param_trusted_name.value, &values);
    return ret;
}
