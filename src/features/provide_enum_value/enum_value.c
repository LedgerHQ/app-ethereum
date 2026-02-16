#include "enum_value.h"
#include "public_keys.h"
#include "utils.h"
#include "ui_utils.h"
#include "app_mem_utils.h"
#include "proxy_info.h"
#include "hash_bytes.h"
#include "tlv_apdu.h"

#define STRUCT_VERSION 0x01

static s_enum_value_entry *g_enum_value_list = NULL;

/**
 * @brief Parse the VERSION value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_version(const tlv_data_t *data, s_enum_value_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, STRUCT_VERSION);
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_chain_id(const tlv_data_t *data, s_enum_value_ctx *context) {
    return tlv_get_chain_id(data, &context->entry.chain_id);
}

/**
 * @brief Parse the CONTRACT_ADDR value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_contract_addr(const tlv_data_t *data, s_enum_value_ctx *context) {
    return tlv_get_address(data, (uint8_t *) context->entry.contract_addr, false);
}

/**
 * @brief Parse the SELECTOR value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_selector(const tlv_data_t *data, s_enum_value_ctx *context) {
    return tlv_get_selector(data, context->entry.selector, sizeof(context->entry.selector));
}

/**
 * @brief Parse the ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_id(const tlv_data_t *data, s_enum_value_ctx *context) {
    if (!tlv_get_uint8(data, &context->entry.id, 0, UINT8_MAX)) {
        PRINTF("ID: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the VALUE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_value(const tlv_data_t *data, s_enum_value_ctx *context) {
    if (!tlv_get_uint8(data, &context->entry.value, 0, UINT8_MAX)) {
        PRINTF("VALUE: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the NAME value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_name(const tlv_data_t *data, s_enum_value_ctx *context) {
    if (!get_string_from_tlv_data(data,
                                  (char *) context->entry.name,
                                  1,
                                  sizeof(context->entry.name) - 1)) {
        PRINTF("NAME: failed to extract\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_signature(const tlv_data_t *data, s_enum_value_ctx *context) {
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

// Define TLV tags for Enum Value
#define ENUM_VALUE_TAGS(X)                                               \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)             \
    X(0x01, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)           \
    X(0x02, TAG_CONTRACT_ADDR, handle_contract_addr, ENFORCE_UNIQUE_TAG) \
    X(0x03, TAG_SELECTOR, handle_selector, ENFORCE_UNIQUE_TAG)           \
    X(0x04, TAG_ID, handle_id, ENFORCE_UNIQUE_TAG)                       \
    X(0x05, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)                 \
    X(0x06, TAG_NAME, handle_name, ENFORCE_UNIQUE_TAG)                   \
    X(0xff, TAG_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration of common handler
static bool enum_value_common_handler(const tlv_data_t *data, s_enum_value_ctx *context);

// Generate TLV parser for Enum Value
DEFINE_TLV_PARSER(ENUM_VALUE_TAGS, &enum_value_common_handler, enum_value_tlv_parser)

/**
 * @brief Common handler called for all tags to hash them (except signature).
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool enum_value_common_handler(const tlv_data_t *data, s_enum_value_ctx *context) {
    if (data->tag != TAG_SIGNATURE) {
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
static bool verify_signature(const s_enum_value_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (finalize_hash((cx_hash_t *) &context->hash_ctx, hash, sizeof(hash)) != true) {
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_CALLDATA,
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
static bool verify_fields(const s_enum_value_ctx *context) {
    return TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                   TAG_VERSION,
                                   TAG_CHAIN_ID,
                                   TAG_CONTRACT_ADDR,
                                   TAG_SELECTOR,
                                   TAG_ID,
                                   TAG_VALUE,
                                   TAG_NAME,
                                   TAG_SIGNATURE);
}

/**
 * @brief Wrapper to parse Enum Value TLV payload.
 *
 * @param[in] buf TLV buffer
 * @param[out] context Enum value context
 * @return whether parsing was successful
 */
bool handle_enum_value_tlv_payload(const buffer_t *buf, s_enum_value_ctx *context) {
    return enum_value_tlv_parser(buf, context, &context->received_tags);
}

bool verify_enum_value_struct(const s_enum_value_ctx *context) {
    s_enum_value_entry *entry;

    if (!verify_fields(context)) {
        PRINTF("Error: Missing mandatory fields in descriptor!\n");
        return false;
    }

    if (!verify_signature(context)) {
        PRINTF("Error: Signature verification failed for descriptor!\n");
        return false;
    }

    if ((entry = APP_MEM_ALLOC(sizeof(*entry))) == NULL) {
        PRINTF("Error: Not enough memory!\n");
        return false;
    }

    memcpy(entry, &context->entry, sizeof(*entry));
    flist_push_back((flist_node_t **) &g_enum_value_list, (flist_node_t *) entry);
    return true;
}

static bool is_matching_enum(const s_enum_value_entry *node,
                             const uint64_t *chain_id,
                             const uint8_t *contract_addr,
                             const uint8_t *selector,
                             uint8_t id,
                             uint8_t value) {
    const uint8_t *proxy_implem;

    proxy_implem = get_implem_contract(chain_id, contract_addr, selector);
    if ((node != NULL) && (*chain_id == node->chain_id) &&
        (memcmp((proxy_implem != NULL) ? proxy_implem : contract_addr,
                node->contract_addr,
                ADDRESS_LENGTH) == 0) &&
        (memcmp(selector, node->selector, SELECTOR_SIZE) == 0) && (id == node->id) &&
        (value == node->value)) {
        return true;
    }
    return false;
}

const s_enum_value_entry *get_matching_enum(const uint64_t *chain_id,
                                            const uint8_t *contract_addr,
                                            const uint8_t *selector,
                                            uint8_t id,
                                            uint8_t value) {
    for (const flist_node_t *tmp = (flist_node_t *) g_enum_value_list; tmp != NULL;
         tmp = tmp->next) {
        if (is_matching_enum((s_enum_value_entry *) tmp,
                             chain_id,
                             contract_addr,
                             selector,
                             id,
                             value)) {
            return (s_enum_value_entry *) tmp;
        }
    }
    return NULL;
}

static void delete_enum_value(s_enum_value_entry *node) {
    APP_MEM_FREE(node);
}

void enum_value_cleanup(void) {
    flist_clear((flist_node_t **) &g_enum_value_list, (f_list_node_del) &delete_enum_value);
    g_enum_value_list = NULL;
}
