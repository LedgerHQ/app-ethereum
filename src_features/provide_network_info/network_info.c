#include "cmd_network_info.h"
#include "network_info.h"
#include "utils.h"
#include "read.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "ui_utils.h"
#include "mem_utils.h"

#define TYPE_DYNAMIC_NETWORK   0x08
#define NETWORK_STRUCT_VERSION 0x01

#define BLOCKCHAIN_FAMILY_ETHEREUM 0x01

// Tags are defined here:
// https://ledgerhq.atlassian.net/wiki/spaces/FW/pages/5039292480/Dynamic+Networks
enum {
    TAG_STRUCTURE_TYPE = 0x01,
    TAG_STRUCTURE_VERSION = 0x02,
    TAG_BLOCKCHAIN_FAMILY = 0x51,
    TAG_CHAIN_ID = 0x23,
    TAG_NETWORK_NAME = 0x52,
    TAG_TICKER = 0x24,
    TAG_NETWORK_ICON_HASH = 0x53,
    TAG_DER_SIGNATURE = 0x15,
};

// Global variable to store the current slot
uint8_t g_current_network_slot = 0;
// Temporary buffer for icon bitmap hash
uint8_t *g_network_icon_hash = NULL;
// Global structure to store the dynamic network information
network_info_t *DYNAMIC_NETWORK_INFO[MAX_DYNAMIC_NETWORKS] = {0};

/**
 * @brief Parse the STRUCTURE_TYPE value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_struct_type(const s_tlv_data *data, s_network_info_ctx *context) {
    (void) context;
    if (data->length != sizeof(uint8_t)) {
        return false;
    }
    return data->value[0] == TYPE_DYNAMIC_NETWORK;
}

/**
 * @brief Parse the STRUCTURE_VERSION value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_struct_version(const s_tlv_data *data, s_network_info_ctx *context) {
    (void) context;
    if (data->length != sizeof(uint8_t)) {
        return false;
    }
    return data->value[0] == NETWORK_STRUCT_VERSION;
}

/**
 * @brief Parse the BLOCKCHAIN_FAMILY value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_blockchain_family(const s_tlv_data *data, s_network_info_ctx *context) {
    (void) context;
    if (data->length != sizeof(uint8_t)) {
        return false;
    }
    return data->value[0] == BLOCKCHAIN_FAMILY_ETHEREUM;
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_chain_id(const s_tlv_data *data, s_network_info_ctx *context) {
    uint64_t chain_id;
    uint8_t buf[sizeof(chain_id)];
    uint64_t max_range;

    (void) context;
    if (data->length > sizeof(buf)) {
        return false;
    }
    // Check if the chain ID is supported
    // https://github.com/ethereum/EIPs/blob/master/EIPS/eip-2294.md
    max_range = 0x7FFFFFFFFFFFFFDB;
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    chain_id = read_u64_be(buf, 0);
    // Check if the chain_id is supported
    if ((chain_id > max_range) || (chain_id == 0)) {
        PRINTF("Unsupported chain ID: %u\n", chain_id);
        return false;
    }
    context->network.chain_id = chain_id;
    return true;
}

/**
 * @brief Parse the NETWORK_NAME value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_name(const s_tlv_data *data, s_network_info_ctx *context) {
    (void) context;
    if (data->length >= sizeof(context->network.name)) {
        return false;
    }
    // Check if the name is printable
    if (!check_name(data->value, data->length)) {
        PRINTF("NETWORK_NAME is not printable!\n");
        return false;
    }
    memcpy(context->network.name, data->value, data->length);
    context->network.name[data->length] = '\0';
    return true;
}

/**
 * @brief Parse the NETWORK_TICKER value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_ticker(const s_tlv_data *data, s_network_info_ctx *context) {
    (void) context;
    if (data->length >= sizeof(context->network.ticker)) {
        return false;
    }
    // Check if the ticker is printable
    if (!check_name(data->value, data->length)) {
        PRINTF("NETWORK_TICKER is not printable!\n");
        return false;
    }
    memcpy(context->network.ticker, data->value, data->length);
    context->network.ticker[data->length] = '\0';
    return true;
}

/**
 * @brief Parse the NETWORK_ICON_HASH value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_icon_hash(const s_tlv_data *data, s_network_info_ctx *context) {
    if (data->length > CX_SHA256_SIZE) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, context->icon_hash, sizeof(context->icon_hash));
    return true;
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_signature(const s_tlv_data *data, s_network_info_ctx *context) {
    if (data->length > sizeof(context->signature)) {
        return false;
    }
    context->signature_length = data->length;
    memcpy(context->signature, data->value, data->length);
    return true;
}

/**
 * @brief Handle a TLV structure from the payload.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
bool handle_network_info_struct(const s_tlv_data *data, s_network_info_ctx *context) {
    bool ret;

    switch (data->tag) {
        case TAG_STRUCTURE_TYPE:
            ret = handle_struct_type(data, context);
            break;
        case TAG_STRUCTURE_VERSION:
            ret = handle_struct_version(data, context);
            break;
        case TAG_BLOCKCHAIN_FAMILY:
            ret = handle_blockchain_family(data, context);
            break;
        case TAG_CHAIN_ID:
            ret = handle_chain_id(data, context);
            break;
        case TAG_NETWORK_NAME:
            ret = handle_name(data, context);
            break;
        case TAG_TICKER:
            ret = handle_ticker(data, context);
            break;
        case TAG_NETWORK_ICON_HASH:
            ret = handle_icon_hash(data, context);
            break;
        case TAG_DER_SIGNATURE:
            ret = handle_signature(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = true;
    }
    if (ret && (data->tag != TAG_DER_SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    }
    return ret;
}

/**
 * @brief Verify the struct
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context struct context
 * @return whether it was successful
 */
bool verify_network_info_struct(const s_network_info_ctx *context) {
    uint8_t hash[CX_SHA256_SIZE];

    if (cx_hash_no_throw((cx_hash_t *) &context->hash_ctx,
                         CX_LAST,
                         NULL,
                         0,
                         hash,
                         CX_SHA256_SIZE) != CX_OK) {
        return false;
    }

    if (check_signature_with_pubkey("Dynamic Network",
                                    hash,
                                    sizeof(hash),
                                    NULL,
                                    0,
                                    CERTIFICATE_PUBLIC_KEY_USAGE_NETWORK,
                                    (uint8_t *) context->signature,
                                    context->signature_length) != CX_OK) {
        return false;
    }

    // Check if the chain ID is already registered, if so delete it silently to prevent duplicates
    for (int i = 0; i < MAX_DYNAMIC_NETWORKS; ++i) {
        if ((DYNAMIC_NETWORK_INFO[i]) &&
            (DYNAMIC_NETWORK_INFO[i]->chain_id == context->network.chain_id)) {
            PRINTF("Network information already exist... Deleting it first!\n");
            network_info_cleanup(i);
            break;
        }
    }

    // Free if already allocated, and reallocate the new size
    if (mem_buffer_allocate((void **) &(DYNAMIC_NETWORK_INFO[g_current_network_slot]),
                            sizeof(network_info_t)) == false) {
        PRINTF("Memory allocation failed for icon hash\n");
        return false;
    }
    // Copy the network information to the global array
    memcpy(DYNAMIC_NETWORK_INFO[g_current_network_slot], &context->network, sizeof(network_info_t));

    // Check if the icon hash is provided
    explicit_bzero(hash, sizeof(hash));
    if (memcmp(hash, context->icon_hash, CX_SHA256_SIZE) == 0) {
        PRINTF("/!\\ Icon hash is not provided!\n");
        // Prepare for next slot
        g_current_network_slot = (g_current_network_slot + 1) % MAX_DYNAMIC_NETWORKS;
        return true;
    }

    // Copy the icon hash to the global array
    if (mem_buffer_allocate((void **) &g_network_icon_hash, CX_SHA256_SIZE) == false) {
        PRINTF("Memory allocation failed for icon hash\n");
        return false;
    }
    memcpy(g_network_icon_hash, context->icon_hash, CX_SHA256_SIZE);
    return true;
}
