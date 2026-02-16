#include "network_info.h"
#include "app_mem_utils.h"
#include "network_icon.h"
#include "utils.h"
#include "read.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "ui_utils.h"
#include "mem_utils.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

#define TYPE_DYNAMIC_NETWORK   0x08
#define NETWORK_STRUCT_VERSION 0x01

#define BLOCKCHAIN_FAMILY_ETHEREUM 0x01

typedef struct {
    network_info_t network;
    uint8_t icon_hash[CX_SHA256_SIZE];
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
    TLV_reception_t received_tags;
} s_network_info_ctx;

// Temporary buffer for icon bitmap hash
uint8_t *g_network_icon_hash = NULL;
// Global list to store the dynamic network information
flist_node_t *g_dynamic_network_list = NULL;
// Pointer to the last added network (for icon association)
network_info_t *g_last_added_network = NULL;

/**
 * @brief Parse the STRUCTURE_TYPE value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_struct_type(const tlv_data_t *data, s_network_info_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_type(data, TYPE_DYNAMIC_NETWORK);
}

/**
 * @brief Parse the STRUCTURE_VERSION value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_struct_version(const tlv_data_t *data, s_network_info_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, NETWORK_STRUCT_VERSION);
}

/**
 * @brief Parse the BLOCKCHAIN_FAMILY value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_blockchain_family(const tlv_data_t *data, s_network_info_ctx *context) {
    UNUSED(context);
    if (!tlv_check_uint8(data, BLOCKCHAIN_FAMILY_ETHEREUM)) {
        PRINTF("BLOCKCHAIN_FAMILY: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_chain_id(const tlv_data_t *data, s_network_info_ctx *context) {
    return tlv_get_chain_id(data, &context->network.chain_id);
}

/**
 * @brief Parse the NETWORK_NAME value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_name(const tlv_data_t *data, s_network_info_ctx *context) {
    if (!tlv_get_printable_string(data,
                                  (char *) context->network.name,
                                  0,
                                  sizeof(context->network.name))) {
        PRINTF("NETWORK_NAME: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the NETWORK_TICKER value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_ticker(const tlv_data_t *data, s_network_info_ctx *context) {
    if (!tlv_get_printable_string(data,
                                  (char *) context->network.ticker,
                                  0,
                                  sizeof(context->network.ticker))) {
        PRINTF("NETWORK_TICKER: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the NETWORK_ICON_HASH value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_icon_hash(const tlv_data_t *data, s_network_info_ctx *context) {
    return tlv_get_hash(data, (char *) context->icon_hash);
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool handle_signature(const tlv_data_t *data, s_network_info_ctx *context) {
    buffer_t sig = {0};
    if (!get_buffer_from_tlv_data(data,
                                  &sig,
                                  ECDSA_SIGNATURE_MIN_LENGTH,
                                  ECDSA_SIGNATURE_MAX_LENGTH)) {
        PRINTF("DER_SIGNATURE: failed to extract\n");
        return false;
    }
    context->sig_size = sig.size;
    context->sig = sig.ptr;
    return true;
}

// Define TLV tags for Network Info
// Tags are defined here:
// https://ledgerhq.atlassian.net/wiki/spaces/FW/pages/5039292480/Dynamic+Networks
#define NETWORK_INFO_TAGS(X)                                                     \
    X(0x01, TAG_STRUCTURE_TYPE, handle_struct_type, ENFORCE_UNIQUE_TAG)          \
    X(0x02, TAG_STRUCTURE_VERSION, handle_struct_version, ENFORCE_UNIQUE_TAG)    \
    X(0x51, TAG_BLOCKCHAIN_FAMILY, handle_blockchain_family, ENFORCE_UNIQUE_TAG) \
    X(0x23, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)                   \
    X(0x52, TAG_NETWORK_NAME, handle_name, ENFORCE_UNIQUE_TAG)                   \
    X(0x24, TAG_TICKER, handle_ticker, ENFORCE_UNIQUE_TAG)                       \
    X(0x53, TAG_NETWORK_ICON_HASH, handle_icon_hash, ENFORCE_UNIQUE_TAG)         \
    X(0x15, TAG_DER_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration of common handler
static bool network_common_handler(const tlv_data_t *data, s_network_info_ctx *context);

// Generate TLV parser for Network Info
DEFINE_TLV_PARSER(NETWORK_INFO_TAGS, &network_common_handler, network_info_tlv_parser)

/**
 * @brief Common handler called for all tags to hash them (except signature).
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool network_common_handler(const tlv_data_t *data, s_network_info_ctx *context) {
    if (data->tag != TAG_DER_SIGNATURE) {
        hash_nbytes(data->raw.ptr, data->raw.size, (cx_hash_t *) &context->hash_ctx);
    }
    return true;
}

/**
 * @brief Verify the payload signature
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context struct context
 * @return whether it was successful
 */
static bool verify_signature(const s_network_info_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (finalize_hash((cx_hash_t *) &context->hash_ctx, hash, sizeof(hash)) != true) {
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_NETWORK,
                                    (uint8_t *) context->sig,
                                    context->sig_size) != true) {
        return false;
    }

    return true;
}

/**
 * @brief Verify the received fields
 *
 * Check the mandatory fields are present
 *
 * @param[in] context struct context
 * @return whether it was successful
 */
static bool verify_fields(const s_network_info_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                   TAG_STRUCTURE_TYPE,
                                   TAG_STRUCTURE_VERSION,
                                   TAG_BLOCKCHAIN_FAMILY,
                                   TAG_CHAIN_ID,
                                   TAG_NETWORK_NAME,
                                   TAG_TICKER,
                                   TAG_DER_SIGNATURE);
}

/**
 * @brief Print the registered network.
 *
 * Only for debug purpose.
 */
static void print_network_info(void) {
    if (g_last_added_network == NULL) {
        return;  // Nothing to print if no network is registered
    }
    PRINTF("****************************************************************************\n");
    PRINTF("[NETWORK] - Registered: '%s' ('%s'), for chain_id %llu\n",
           g_last_added_network->name,
           g_last_added_network->ticker,
           g_last_added_network->chain_id);
}

/**
 * @brief Verify the struct
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context struct context
 * @return whether it was successful
 */
static bool verify_network_info_struct(const s_network_info_ctx *context) {
    if (!verify_fields(context)) {
        PRINTF("Error: Missing mandatory fields in Network descriptor!\n");
        return false;
    }
    if (!verify_signature(context)) {
        PRINTF("Error: Signature verification failed for Network descriptor!\n");
        return false;
    }
    return true;
}

/**
 * @brief Append the network information to the global list
 *
 * @param[in] context struct context
 * @return whether it was successful
 */
static bool append_network_info(const s_network_info_ctx *context) {
    // Check if the chain ID is already registered, if so delete it to prevent duplicates
    network_info_t *existing = find_dynamic_network_by_chain_id(context->network.chain_id);
    if (existing != NULL) {
        PRINTF("Network information already exist... Deleting it first!\n");
        // Remove from list and cleanup
        const uint8_t *bitmap = existing->icon.bitmap;
        if (bitmap != NULL) {
            APP_MEM_FREE_AND_NULL((void **) &bitmap);
        }
        flist_remove(&g_dynamic_network_list, (flist_node_t *) existing, NULL);
        APP_MEM_FREE_AND_NULL((void **) &existing);
    }
    // Do not track the allocation in logs, because this buffer is expected to stay allocated
    network_info_t *new_network = NULL;
    if (APP_MEM_PERMANENT((void **) &new_network, sizeof(network_info_t)) == false) {
        PRINTF("Memory allocation failed for network info\n");
        return false;
    }

    // Copy the network information
    memcpy(new_network, &context->network, sizeof(network_info_t));

    // Add to the list
    flist_push_back(&g_dynamic_network_list, (flist_node_t *) new_network);

    // Keep track of last added network for icon association
    g_last_added_network = new_network;
    return true;
}

/**
 * @brief Prepare the network icon by storing the expected hash and
 * allocating the buffer for the icon bitmap
 *
 * @param[in] context struct context
 * @return whether it was successful
 */
static bool prepare_network_icon(const s_network_info_ctx *context) {
    // Check if the icon hash is provided
    if (allzeroes(context->icon_hash, CX_SHA256_SIZE) == 1) {
        PRINTF("/!\\ Icon hash is not provided!\n");
        return true;
    }
    // Store the expected icon hash for later verification when the icon bitmap is received
    if (APP_MEM_CALLOC((void **) &g_network_icon_hash, CX_SHA256_SIZE) == false) {
        PRINTF("Memory allocation failed for icon hash\n");
        return false;
    }
    memcpy(g_network_icon_hash, context->icon_hash, CX_SHA256_SIZE);
    return true;
}

/**
 * @brief Verify the struct
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context struct context
 * @return whether it was successful
 */
static bool add_network_info(const s_network_info_ctx *context) {
    if (!append_network_info(context)) {
        PRINTF("Error: Failed to append network information!\n");
        return false;
    }

    if (!prepare_network_icon(context)) {
        PRINTF("Error: Failed to prepare network icon!\n");
        return false;
    }
    print_network_info();
    return true;
}

/**
 * @brief Handle Network Configuration TLV payload.
 *
 * @param[in] buf TLV buffer received
 * @return whether it was successful or not
 */
bool handle_network_tlv_payload(const buffer_t *buf) {
    bool ret = false;
    s_network_info_ctx ctx = {0};

    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    // Parse using SDK TLV parser
    ret = network_info_tlv_parser(buf, &ctx, &ctx.received_tags);
    if (ret) {
        ret = verify_network_info_struct(&ctx);
    }
    if (ret) {
        ret = add_network_info(&ctx);
    }
    return ret;
}

/**
 * @brief Cleanup Network Configuration context.
 *
 * @param[in] network Network to cleanup (NULL = cleanup all networks)
 */
void network_info_cleanup(network_info_t *network) {
    if (network == NULL) {
        // Cleanup all networks in the list
        flist_node_t *node = g_dynamic_network_list;
        while (node != NULL) {
            flist_node_t *next = node->next;
            network_info_t *net_info = (network_info_t *) node;

            // Free the icon bitmap if allocated
            const uint8_t *bitmap = net_info->icon.bitmap;
            if (bitmap != NULL) {
                APP_MEM_FREE_AND_NULL((void **) &bitmap);
            }
            // Free the network info structure
            APP_MEM_FREE_AND_NULL((void **) &net_info);
            node = next;
        }
        g_dynamic_network_list = NULL;
        g_last_added_network = NULL;

        // Cleanup temporary icon bitmap (in case one was being received)
        clear_icon();
    } else {
        // Cleanup specific network
        // Free the icon bitmap if allocated
        const uint8_t *bitmap = network->icon.bitmap;
        if (bitmap != NULL) {
            APP_MEM_FREE_AND_NULL((void **) &bitmap);
        }

        // Remove from list
        flist_remove(&g_dynamic_network_list, (flist_node_t *) network, NULL);

        // Free the network info structure
        APP_MEM_FREE_AND_NULL((void **) &network);

        // Reset last added network pointer if it was this network
        if (g_last_added_network == network) {
            g_last_added_network = NULL;
            // Also cleanup temporary buffers associated with this network
            clear_icon();
        }
    }
}
