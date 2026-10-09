#include "enum_value.h"
#include "public_keys.h"
#include "utils.h"
#include "ui_utils.h"
#include "app_mem_utils.h"
#include "hash_bytes.h"
#include "tlv_utils.h"
#include "lcx_ecdsa.h"

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
    if (!tlv_enforce_u8_value(data, STRUCT_VERSION)) {
        PRINTF("Invalid STRUCTURE_VERSION value\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_chain_id(const tlv_data_t *data, s_enum_value_ctx *context) {
    return tlv_get_chain_id(data, &context->entry.operation_key.chain_id);
}

/**
 * @brief Parse the CONTRACT_ADDR value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_contract_addr(const tlv_data_t *data, s_enum_value_ctx *context) {
    return tlv_get_address(data, context->entry.operation_key.contract_addr);
}

/**
 * @brief Parse the SELECTOR value, which tells the value is of a transaction.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_selector(const tlv_data_t *data, s_enum_value_ctx *context) {
    context->entry.operation_key.context = OPERATION_INFO_CTX_SIGN_TX;
    return tlv_get_hash(data,
                        context->entry.operation_key.selector,
                        sizeof(context->entry.operation_key.selector));
}

/**
 * @brief Parse the DOMAIN_SEPARATOR value, of a value of a typed message.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_domain_separator(const tlv_data_t *data, s_enum_value_ctx *context) {
    return tlv_get_hash(data,
                        context->entry.operation_key.domain_separator,
                        sizeof(context->entry.operation_key.domain_separator));
}

/**
 * @brief Parse the PRIMARY_TYPE_HASH value, which tells the value is of a typed message.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_primary_type_hash(const tlv_data_t *data, s_enum_value_ctx *context) {
    context->entry.operation_key.context = OPERATION_INFO_CTX_EIP712;
    return tlv_get_hash(data,
                        context->entry.operation_key.primary_type_hash,
                        sizeof(context->entry.operation_key.primary_type_hash));
}

/**
 * @brief Parse the ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Enum value context
 * @return whether the handling was successful
 */
static bool handle_id(const tlv_data_t *data, s_enum_value_ctx *context) {
    if (!tlv_get_uint8_range(data, &context->entry.id, 0, UINT8_MAX)) {
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
    if (!tlv_get_uint8_range(data, &context->entry.value, 0, UINT8_MAX)) {
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
                                  CX_ECDSA_SHA256_SIG_MIN_ASN1_LENGTH,
                                  CX_ECDSA_SHA256_SIG_MAX_ASN1_LENGTH)) {
        PRINTF("SIGNATURE: failed to extract\n");
        return false;
    }
    context->sig_size = sig.size;
    context->sig = sig.ptr;
    return true;
}

// Define TLV tags for Enum Value
#define ENUM_VALUE_TAGS(X)                                                       \
    X(0x00, TAG_VERSION, handle_version, ENFORCE_UNIQUE_TAG)                     \
    X(0x01, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)                   \
    X(0x02, TAG_CONTRACT_ADDR, handle_contract_addr, ENFORCE_UNIQUE_TAG)         \
    X(0x03, TAG_SELECTOR, handle_selector, ENFORCE_UNIQUE_TAG)                   \
    X(0x04, TAG_ID, handle_id, ENFORCE_UNIQUE_TAG)                               \
    X(0x05, TAG_VALUE, handle_value, ENFORCE_UNIQUE_TAG)                         \
    X(0x06, TAG_NAME, handle_name, ENFORCE_UNIQUE_TAG)                           \
    X(0x07, TAG_DOMAIN_SEPARATOR, handle_domain_separator, ENFORCE_UNIQUE_TAG)   \
    X(0x08, TAG_PRIMARY_TYPE_HASH, handle_primary_type_hash, ENFORCE_UNIQUE_TAG) \
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
                                    NULL,
                                    0,
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
    TLV_reception_t tags = context->received_tags;
    bool of_tx = TLV_CHECK_RECEIVED_TAGS(tags, TAG_CHAIN_ID) ||
                 TLV_CHECK_RECEIVED_TAGS(tags, TAG_CONTRACT_ADDR) ||
                 TLV_CHECK_RECEIVED_TAGS(tags, TAG_SELECTOR);
    bool of_message = TLV_CHECK_RECEIVED_TAGS(tags, TAG_DOMAIN_SEPARATOR) ||
                      TLV_CHECK_RECEIVED_TAGS(tags, TAG_PRIMARY_TYPE_HASH);

    if (!TLV_CHECK_RECEIVED_TAGS(tags, TAG_VERSION, TAG_ID, TAG_VALUE, TAG_NAME, TAG_SIGNATURE)) {
        return false;
    }
    // the value is attached to a function of a contract or to a primary type of a domain, whole,
    // never to both
    if (of_tx == of_message) {
        return false;
    }
    if (of_tx) {
        return TLV_CHECK_RECEIVED_TAGS(tags, TAG_CHAIN_ID, TAG_CONTRACT_ADDR, TAG_SELECTOR);
    }
    return TLV_CHECK_RECEIVED_TAGS(tags, TAG_DOMAIN_SEPARATOR, TAG_PRIMARY_TYPE_HASH);
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

const s_enum_value_entry *get_matching_enum(const s_operation_key *operation_key,
                                            uint8_t id,
                                            uint8_t value) {
    for (const flist_node_t *tmp = (flist_node_t *) g_enum_value_list; tmp != NULL;
         tmp = tmp->next) {
        const s_enum_value_entry *node = (const s_enum_value_entry *) tmp;

        if (operation_key_matches(&node->operation_key, operation_key) && (id == node->id) &&
            (value == node->value)) {
            return node;
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
