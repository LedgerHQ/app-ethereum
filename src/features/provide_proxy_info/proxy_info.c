#include "proxy_info.h"
#include "utils.h"
#include "challenge.h"
#include "public_keys.h"
#include "ui_utils.h"
#include "mem_utils.h"
#include "hash_bytes.h"
#include "tlv_apdu.h"

#define TYPE_PROXY_INFO 0x26
#define STRUCT_VERSION  0x01

typedef enum {
    DELEGATION_TYPE_PROXY = 0,
    DELEGATION_TYPE_ISSUED_FROM_FACTORY = 1,
    DELEGATION_TYPE_DELEGATOR = 2,
    DELEGATION_TYPE_MAX,
} e_delegation_type;

static s_proxy_info *g_proxy_info = NULL;

/**
 * @brief Parse the STRUCT_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_struct_type(const tlv_data_t *data, s_proxy_info_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_type(data, TYPE_PROXY_INFO);
}

/**
 * @brief Parse the STRUCT_VERSION value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_struct_version(const tlv_data_t *data, s_proxy_info_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, STRUCT_VERSION);
}

/**
 * @brief Parse the CHALLENGE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_challenge(const tlv_data_t *data, s_proxy_info_ctx *context) {
    UNUSED(context);
    return tlv_check_challenge(data);
}

/**
 * @brief Parse the ADDRESS value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_address(const tlv_data_t *data, s_proxy_info_ctx *context) {
    return tlv_get_address(data, context->proxy_info.address, false);
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_chain_id(const tlv_data_t *data, s_proxy_info_ctx *context) {
    return tlv_get_chain_id(data, &context->proxy_info.chain_id);
}

/**
 * @brief Parse the SELECTOR value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_selector(const tlv_data_t *data, s_proxy_info_ctx *context) {
    return tlv_get_selector(data,
                            context->proxy_info.selector,
                            sizeof(context->proxy_info.selector));
}

/**
 * @brief Parse the IMPLEM_ADDRESS value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_implem_address(const tlv_data_t *data, s_proxy_info_ctx *context) {
    return tlv_get_address(data, context->proxy_info.implem_address, false);
}

/**
 * @brief Parse the DELEGATION_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_delegation_type(const tlv_data_t *data, s_proxy_info_ctx *context) {
    UNUSED(context);
    uint8_t value = 0;
    if (!tlv_get_uint8(data, &value, 0, DELEGATION_TYPE_MAX - 1)) {
        PRINTF("DELEGATION_TYPE: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Proxy info context
 * @return whether the handling was successful
 */
static bool handle_signature(const tlv_data_t *data, s_proxy_info_ctx *context) {
    buffer_t sig = {0};
    if (!get_buffer_from_tlv_data(data,
                                  &sig,
                                  ECDSA_SIGNATURE_MIN_LENGTH,
                                  ECDSA_SIGNATURE_MAX_LENGTH)) {
        PRINTF("SIGNATURE: failed to extract\n");
        return false;
    }
    context->sig_size = sig.size;
    context->sig = sig.ptr;
    return true;
}

// Define TLV tags for Proxy Info
#define PROXY_INFO_TAGS(X)                                                   \
    X(0x01, TAG_STRUCT_TYPE, handle_struct_type, ENFORCE_UNIQUE_TAG)         \
    X(0x02, TAG_STRUCT_VERSION, handle_struct_version, ENFORCE_UNIQUE_TAG)   \
    X(0x12, TAG_CHALLENGE, handle_challenge, ENFORCE_UNIQUE_TAG)             \
    X(0x22, TAG_ADDRESS, handle_address, ENFORCE_UNIQUE_TAG)                 \
    X(0x23, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)               \
    X(0x41, TAG_SELECTOR, handle_selector, ENFORCE_UNIQUE_TAG)               \
    X(0x42, TAG_IMPLEM_ADDRESS, handle_implem_address, ENFORCE_UNIQUE_TAG)   \
    X(0x43, TAG_DELEGATION_TYPE, handle_delegation_type, ENFORCE_UNIQUE_TAG) \
    X(0x15, TAG_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration of common handler
static bool proxy_info_common_handler(const tlv_data_t *data, s_proxy_info_ctx *context);

// Generate TLV parser for Proxy Info
DEFINE_TLV_PARSER(PROXY_INFO_TAGS, &proxy_info_common_handler, proxy_info_tlv_parser)

/**
 * @brief Common handler called for all tags to hash them (except signature).
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool proxy_info_common_handler(const tlv_data_t *data, s_proxy_info_ctx *context) {
    if (data->tag != TAG_SIGNATURE) {
        hash_nbytes(data->raw.ptr, data->raw.size, (cx_hash_t *) &context->struct_hash);
    }
    return true;
}

/**
 * @brief Wrapper to parse Proxy Info TLV payload.
 *
 * @param[in] buf TLV buffer
 * @param[out] context Proxy info context
 * @return whether parsing was successful
 */
bool handle_proxy_info_tlv_payload(const buffer_t *buf, s_proxy_info_ctx *context) {
    return proxy_info_tlv_parser(buf, context, &context->received_tags);
}

bool verify_proxy_info_struct(const s_proxy_info_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (finalize_hash((cx_hash_t *) &context->struct_hash, hash, sizeof(hash)) != true) {
        return false;
    }
    roll_challenge();
    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_TRUSTED_NAME,
                                    (uint8_t *) context->sig,
                                    context->sig_size) != true) {
        return false;
    }
    if (mem_buffer_allocate((void **) &g_proxy_info, sizeof(s_proxy_info)) == false) {
        PRINTF("Error: Not enough memory!\n");
        return false;
    }
    memcpy(g_proxy_info, &context->proxy_info, sizeof(s_proxy_info));
    PRINTF("================== PROXY INFO ====================\n");
    PRINTF("chain ID = %u\n", (uint32_t) g_proxy_info->chain_id);
    PRINTF("address = 0x%.*h\n", sizeof(g_proxy_info->address), g_proxy_info->address);
    PRINTF("implementation address = 0x%.*h\n",
           sizeof(g_proxy_info->implem_address),
           g_proxy_info->implem_address);
    if (g_proxy_info->has_selector) {
        PRINTF("selector = 0x%.*h\n", sizeof(g_proxy_info->selector), g_proxy_info->selector);
    }
    PRINTF("==================================================\n");
    return true;
}

static bool check_proxy_params(const uint64_t *chain_id,
                               const uint8_t *addr,
                               const uint8_t *selector,
                               const uint64_t *ref_chain_id,
                               const uint8_t *ref_addr,
                               const uint8_t *ref_selector) {
    if ((chain_id == NULL) || (*chain_id != *ref_chain_id)) {
        return false;
    }
    if (memcmp(addr, ref_addr, ADDRESS_LENGTH) != 0) {
        return false;
    }
    if (selector != NULL) {
        if (memcmp(selector, ref_selector, CALLDATA_SELECTOR_SIZE) != 0) {
            return false;
        }
    }
    return true;
}

const uint8_t *get_proxy_contract(const uint64_t *chain_id,
                                  const uint8_t *addr,
                                  const uint8_t *selector) {
    if (g_proxy_info == NULL) {
        PRINTF("Error: Proxy info not initialized!\n");
        return NULL;
    }

    if (!check_proxy_params(chain_id,
                            addr,
                            g_proxy_info->has_selector ? selector : NULL,
                            &g_proxy_info->chain_id,
                            g_proxy_info->implem_address,
                            g_proxy_info->selector)) {
        return NULL;
    }
    return g_proxy_info->address;
}

const uint8_t *get_implem_contract(const uint64_t *chain_id,
                                   const uint8_t *addr,
                                   const uint8_t *selector) {
    if (g_proxy_info == NULL) {
        PRINTF("Error: Proxy info not initialized!\n");
        return NULL;
    }

    if (!check_proxy_params(chain_id,
                            addr,
                            g_proxy_info->has_selector ? selector : NULL,
                            &g_proxy_info->chain_id,
                            g_proxy_info->address,
                            g_proxy_info->selector)) {
        return NULL;
    }
    return g_proxy_info->implem_address;
}

void proxy_cleanup(void) {
    mem_buffer_cleanup((void **) &g_proxy_info);
}
