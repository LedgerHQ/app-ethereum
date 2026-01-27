/**
 * @file gtp_param_network.c
 * @brief Generic transaction parser - Network parameter handling
 *
 * This file implements the network parameter parsing and formatting functionality
 * for the generic transaction parser. It handles network chain IDs and converts
 * them to human-readable network names.
 */

#include <string.h>
#include "gtp_param_network.h"
#include "shared_context.h"
#include "utils.h"
#include "network.h"
#include "gtp_field_table.h"
#include "tx_ctx.h"
#include "apdu_constants.h"

/**
 * @brief TLV tags for network parameter structure
 */
enum {
    TAG_VERSION = 0x00, /**< Version tag */
    TAG_VALUE = 0x01,   /**< Value tag */
};

/**
 * @brief Handle the version tag in network parameter structure
 *
 * Parses and validates the version field from TLV data.
 *
 * @param[in] data TLV data containing the version
 * @param[in,out] context Network parameter context to store the version
 * @return true if version was successfully parsed, false otherwise
 */
static bool handle_version(const s_tlv_data *data, s_param_network_context *context) {
    if (data->length != sizeof(context->param->version)) {
        return false;
    }
    context->param->version = data->value[0];  // TODO: what is it used for? Any check somewhere?
    return true;
}

/**
 * @brief Handle the value tag in network parameter structure
 *
 * Parses the network value (chain ID) from TLV data.
 *
 * @param[in] data TLV data containing the network value
 * @param[in,out] context Network parameter context to store the value
 * @return true if value was successfully parsed, false otherwise
 */
static bool handle_value(const s_tlv_data *data, s_param_network_context *context) {
    s_value_context ctx = {0};

    ctx.value = &context->param->value;
    explicit_bzero(ctx.value, sizeof(*ctx.value));
    return tlv_parse(data->value, data->length, (f_tlv_data_handler) &handle_value_struct, &ctx);
}

/**
 * @brief Parse network parameter structure from TLV data
 *
 * Main handler for parsing network parameter TLV structure.
 * Dispatches to appropriate handler based on TLV tag.
 *
 * @param[in] data TLV data to parse
 * @param[in,out] context Network parameter context
 * @return true if parsing was successful, false otherwise
 */
bool handle_param_network_struct(const s_tlv_data *data, s_param_network_context *context) {
    bool ret;

    switch (data->tag) {
        case TAG_VERSION:
            ret = handle_version(data, context);
            break;
        case TAG_VALUE:
            ret = handle_value(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    return ret;
}

/**
 * @brief Format network name to human-readable string from chain ID
 *
 * Converts a chain ID value to the corresponding network name.
 * Validates the chain ID according to EIP-2294 specifications.
 *
 * @param[in] value Parsed value containing the chain ID
 * @param[out] buf Buffer to store the formatted network name
 * @param[in] buf_size Size of the output buffer
 * @return true if formatting was successful, false otherwise
 *
 * @note Chain ID must be a 64-bit unsigned integer
 * @note Chain ID must be in range [1, 0x7FFFFFFFFFFFFFDB] per EIP-2294
 */
static bool format_network_name(const s_parsed_value *value, char *buf, size_t buf_size) {
    uint64_t chain_id = 0;
    uint64_t max_range = 0x7FFFFFFFFFFFFFDB;  // per EIP-2294
    uint16_t sw = SWO_PARAMETER_ERROR_NO_INFO;

    // Check length
    if (value->length != sizeof(uint64_t)) {
        PRINTF("CHAIN_ID Length mismatch!\n");
        return false;
    }
    // Check if the chain ID is supported
    // https://github.com/ethereum/EIPs/blob/master/EIPS/eip-2294.md
    chain_id = u64_from_BE(value->ptr, value->length);
    // Check if the chain_id is supported
    if ((chain_id > max_range) || (chain_id == 0)) {
        PRINTF("Unsupported chain ID: %llu\n", chain_id);
        return false;
    }
    sw = get_network_as_string_from_chain_id(buf, buf_size, chain_id);
    return (sw == SWO_SUCCESS) ? true : false;
}

/**
 * @brief Format network parameter for display
 *
 * Formats the network parameter and adds it to the field table for display.
 * Handles chain ID extraction and conversion to network name.
 *
 * @param[in] param Network parameter to format
 * @param[in] name Field name to display
 * @return true if formatting and field table addition was successful, false otherwise
 *
 * @note Only supports TF_BYTES type family for network values
 * @note The formatted network name is added to the field table with PARAM_TYPE_NETWORK type
 */
bool format_param_network(const s_param_network *param, const char *name) {
    bool ret;
    s_parsed_value_collection collec = {0};
    char *buf = strings.tmp.tmp;
    size_t buf_size = sizeof(strings.tmp.tmp);

    if ((ret = value_get(&param->value, &collec))) {
        for (int i = 0; i < collec.size && ret == true; ++i) {
            if (param->value.type_family == TF_UINT) {
                // TODO: Should we rather use TF_FIXED or TF_UFIXED?
                ret = format_network_name(&collec.value[i], buf, buf_size);
                if (ret) ret = add_to_field_table(PARAM_TYPE_NETWORK, name, buf, NULL);
            } else {
                ret = false;
            }
        }
    }
    value_cleanup(&param->value, &collec);
    return ret;
}
