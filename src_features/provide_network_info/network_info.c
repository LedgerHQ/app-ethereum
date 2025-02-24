#ifdef HAVE_DYNAMIC_NETWORKS

#include <ctype.h>
#include "network_info.h"
#include "apdu_constants.h"
#include "hash_bytes.h"
#include "public_keys.h"

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

#ifdef HAVE_NBGL
uint8_t g_network_icon_hash[MAX_DYNAMIC_NETWORKS][CX_SHA256_SIZE] = {0};
#endif
// Global structure to store the dynamic network information
network_info_t DYNAMIC_NETWORK_INFO[MAX_DYNAMIC_NETWORKS] = {0};

// Macros to check the field length
#define CHECK_FIELD_LENGTH(tag, len, expected)  \
    do {                                        \
        if (len != expected) {                  \
            PRINTF("%s Size mismatch!\n", tag); \
            return APDU_RESPONSE_INVALID_DATA;  \
        }                                       \
    } while (0)
#define CHECK_FIELD_OVERFLOW(tag, field)              \
    do {                                              \
        if (field_len >= sizeof(field)) {             \
            PRINTF("%s Size overflow!\n", tag);       \
            return APDU_RESPONSE_INSUFFICIENT_MEMORY; \
        }                                             \
    } while (0)

// Macro to check the field value
#define CHECK_FIELD_VALUE(tag, value, expected)  \
    do {                                         \
        if (value != expected) {                 \
            PRINTF("%s Value mismatch!\n", tag); \
            return APDU_RESPONSE_INVALID_DATA;   \
        }                                        \
    } while (0)

// Macro to copy the field
#define COPY_FIELD(field)                         \
    do {                                          \
        memmove((void *) field, data, field_len); \
    } while (0)

/**
 * @brief Parse the STRUCTURE_TYPE value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_struct_type(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_LENGTH("STRUCTURE_TYPE", field_len, 1);
    CHECK_FIELD_VALUE("STRUCTURE_TYPE", data[0], TYPE_DYNAMIC_NETWORK);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the STRUCTURE_VERSION value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_struct_version(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_LENGTH("STRUCTURE_VERSION", field_len, 1);
    CHECK_FIELD_VALUE("STRUCTURE_VERSION", data[0], NETWORK_STRUCT_VERSION);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the BLOCKCHAIN_FAMILY value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_blockchain_family(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_LENGTH("BLOCKCHAIN_FAMILY", field_len, 1);
    CHECK_FIELD_VALUE("BLOCKCHAIN_FAMILY", data[0], BLOCKCHAIN_FAMILY_ETHEREUM);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_chain_id(const uint8_t *data, uint16_t field_len) {
    uint64_t chain_id;
    uint64_t max_range;
    uint8_t i;

    CHECK_FIELD_LENGTH("CHAIN_ID", field_len, sizeof(uint64_t));
    // Check if the chain ID is supported
    // https://github.com/ethereum/EIPs/blob/master/EIPS/eip-2294.md
    max_range = 0x7FFFFFFFFFFFFFDB;
    chain_id = u64_from_BE(data, field_len);
    // Check if the chain_id is supported
    if ((chain_id > max_range) || (chain_id == 0)) {
        PRINTF("Unsupported chain ID: %u\n", chain_id);
        return APDU_RESPONSE_INVALID_DATA;
    }
    // Check if the chain_id is already registered
    for (i = 0; i < MAX_DYNAMIC_NETWORKS; i++) {
        if (DYNAMIC_NETWORK_INFO[i].chain_id == chain_id) {
            PRINTF("CHAIN_ID already exist!\n");
            return APDU_RESPONSE_INVALID_DATA;
        }
    }
    DYNAMIC_NETWORK_INFO[g_current_network_slot].chain_id = chain_id;
    return APDU_RESPONSE_OK;
}

/**
 * @brief Check the name is printable.
 *
 * @param[in] data buffer received
 * @param[in] name Name to check
 * @param[in] len Length of the name
 * @return True/False
 */
static bool check_name(const uint8_t *name, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        if (!isprint(name[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Parse the NETWORK_NAME value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_name(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_OVERFLOW("NETWORK_NAME", DYNAMIC_NETWORK_INFO[g_current_network_slot].name);
    // Check if the name is printable
    if (!check_name(data, field_len)) {
        PRINTF("NETWORK_NAME is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(DYNAMIC_NETWORK_INFO[g_current_network_slot].name);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the NETWORK_TICKER value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_ticker(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_OVERFLOW("NETWORK_TICKER", DYNAMIC_NETWORK_INFO[g_current_network_slot].ticker);
    // Check if the ticker is printable
    if (!check_name(data, field_len)) {
        PRINTF("NETWORK_TICKER is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(DYNAMIC_NETWORK_INFO[g_current_network_slot].ticker);
    return APDU_RESPONSE_OK;
}

#ifdef HAVE_NBGL
/**
 * @brief Parse the NETWORK_ICON_HASH value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_icon_hash(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_LENGTH("NETWORK_ICON_HASH", field_len, CX_SHA256_SIZE);
    COPY_FIELD(g_network_icon_hash[g_current_network_slot]);
    return APDU_RESPONSE_OK;
}
#endif

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @param[in] sig_ctx the signature context
 * @return APDU Response code
 */
static uint16_t parse_signature(const uint8_t *data, uint16_t field_len, s_sig_ctx *sig_ctx) {
    sig_ctx->sig_size = field_len;
    sig_ctx->sig = data;
    return APDU_RESPONSE_OK;
}

bool handle_network_info_struct(const s_tlv_data *data, s_sig_ctx *context) {
    uint16_t ret = APDU_RESPONSE_INTERNAL_ERROR;

    switch (data->tag) {
        case TAG_STRUCTURE_TYPE:
            ret = parse_struct_type(data->value, data->length);
            break;
        case TAG_STRUCTURE_VERSION:
            ret = parse_struct_version(data->value, data->length);
            break;
        case TAG_BLOCKCHAIN_FAMILY:
            ret = parse_blockchain_family(data->value, data->length);
            break;
        case TAG_CHAIN_ID:
            ret = parse_chain_id(data->value, data->length);
            break;
        case TAG_NETWORK_NAME:
            ret = parse_name(data->value, data->length);
            break;
        case TAG_TICKER:
            ret = parse_ticker(data->value, data->length);
            break;
        case TAG_NETWORK_ICON_HASH:
#ifdef HAVE_NBGL
            ret = parse_icon_hash(data->value, data->length);
#else
            ret = APDU_RESPONSE_OK;
#endif
            break;
        case TAG_DER_SIGNATURE:
            ret = parse_signature(data->value, data->length, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = APDU_RESPONSE_OK;
    }
    if ((ret == APDU_RESPONSE_OK) && (data->tag != TAG_DER_SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    }
    return (ret == APDU_RESPONSE_OK);
}

/**
 * @brief Verify the payload signature
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] sig_ctx the signature context
 * @return whether it was successful
 */
bool verify_network_info_struct(s_sig_ctx *sig_ctx) {
    uint8_t hash[INT256_LENGTH];

    if (cx_hash_no_throw((cx_hash_t *) &sig_ctx->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH) !=
        CX_OK) {
        return false;
    }

    if (check_signature_with_pubkey("Dynamic Network",
                                    hash,
                                    sizeof(hash),
                                    NULL,
                                    0,
#ifdef HAVE_LEDGER_PKI
                                    CERTIFICATE_PUBLIC_KEY_USAGE_NETWORK,
#endif
                                    (uint8_t *) (sig_ctx->sig),
                                    sig_ctx->sig_size) != CX_OK) {
        return false;
    }
    return true;
}

#endif  // HAVE_DYNAMIC_NETWORKS
