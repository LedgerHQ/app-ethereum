#include <string.h>
#include <ctype.h>
#include "os_utils.h"
#include "os_pic.h"
#include "network.h"
#include "network_dynamic.h"
#include "shared_context.h"
#include "common_utils.h"
#include "apdu_constants.h"
#include "write.h"
#include "hash_bytes.h"
#include "lcx_hash.h"
#include "lcx_sha256.h"
#include "public_keys.h"
#ifdef HAVE_LEDGER_PKI
#include "os_pki.h"
#endif
#include "tlv.h"

#define P2_NETWORK_CONFIG 0x00
#define P2_NETWORK_ICON   0x01
#define P2_GET_INFO       0x02

#define MAX_ICON_LEN 1024

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

// Signature context structure
typedef struct {
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
} s_sig_ctx;

// Global variable to store the current slot
static uint8_t g_current_slot = 0;

#ifdef HAVE_NBGL
typedef struct {
    uint16_t received_size;
    uint16_t expected_size;
} network_payload_t;

// Global variable to store the network icons
typedef struct {
    uint8_t bitmap[MAX_ICON_LEN];
    uint8_t hash[CX_SHA256_SIZE];
} network_icon_t;
// Global structure to store the network icons
static network_icon_t g_network_icon[MAX_DYNAMIC_NETWORKS] = {0};
// Global structure to temporary store the network icon APDU
static network_payload_t g_icon_payload = {0};
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

    DYNAMIC_NETWORK_INFO[g_current_slot].chain_id = chain_id;
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
    CHECK_FIELD_OVERFLOW("NETWORK_NAME", DYNAMIC_NETWORK_INFO[g_current_slot].name);
    // Check if the name is printable
    if (!check_name(data, field_len)) {
        PRINTF("NETWORK_NAME is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(DYNAMIC_NETWORK_INFO[g_current_slot].name);
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
    CHECK_FIELD_OVERFLOW("NETWORK_TICKER", DYNAMIC_NETWORK_INFO[g_current_slot].ticker);
    // Check if the ticker is printable
    if (!check_name(data, field_len)) {
        PRINTF("NETWORK_TICKER is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(DYNAMIC_NETWORK_INFO[g_current_slot].ticker);
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
    COPY_FIELD(g_network_icon[g_current_slot].hash);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Check the NETWORK_ICON header.
 *
 * @param[in] data buffer received
 * @param[in] length Length of the field value
 * @return APDU Response code
 */
static uint16_t check_icon_header(const uint8_t *data, uint16_t length, uint16_t *buffer_size) {
    // The chunk starts by the Image Header (8 Bytes):
    //   - Width (2 Bytes)
    //   - Height (2 Bytes)
    //   - BPP (1 Byte)
    //   - Img buffer size (3 Bytes)
    if (length < 8) {
        PRINTF("NETWORK_ICON header length mismatch (%d)!\n", length);
        return APDU_RESPONSE_INVALID_DATA;
    }
    *buffer_size = 8 + data[5] + (data[6] << 8) + (data[7] << 16);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Print the registered network icon.
 *
 * Only for debug purpose.
 *
 */
static void print_icon_info(void) {
    PRINTF("****************************************************************************\n");
    PRINTF("[NETWORK_ICON] - Registered in slot %d: icon %dx%d (BPP %d)\n",
           g_current_slot,
           DYNAMIC_NETWORK_INFO[g_current_slot].icon.width,
           DYNAMIC_NETWORK_INFO[g_current_slot].icon.height,
           DYNAMIC_NETWORK_INFO[g_current_slot].icon.bpp);
}

/**
 * @brief Parse and check the NETWORK_ICON value.
 *
 * @return APDU Response code
 */
static uint16_t parse_icon_buffer(void) {
    uint16_t img_len = 0;
    uint16_t sw = APDU_RESPONSE_UNKNOWN;
    uint8_t digest[CX_SHA256_SIZE];
    const uint8_t *data = g_network_icon[g_current_slot].bitmap;
    const uint16_t field_len = g_icon_payload.received_size;
    cx_err_t error = CX_INTERNAL_ERROR;

    // Check the icon header
    sw = check_icon_header(data, field_len, &img_len);
    if (sw != APDU_RESPONSE_OK) {
        return sw;
    }
    CHECK_FIELD_LENGTH("NETWORK_ICON", field_len, img_len);
    CHECK_FIELD_OVERFLOW("NETWORK_ICON", g_network_icon[g_current_slot].bitmap);

    // Check icon hash
    CX_CHECK(cx_sha256_hash(data, field_len, digest));
    if (memcmp(digest, g_network_icon[g_current_slot].hash, CX_SHA256_SIZE) != 0) {
        PRINTF("NETWORK_ICON hash mismatch!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    DYNAMIC_NETWORK_INFO[g_current_slot].icon.bitmap =
        (const uint8_t *) g_network_icon[g_current_slot].bitmap;
    DYNAMIC_NETWORK_INFO[g_current_slot].icon.width = U2LE(data, 0);
    DYNAMIC_NETWORK_INFO[g_current_slot].icon.height = U2LE(data, 2);
    // BPP is stored in the upper 4 bits of the 5th byte
    DYNAMIC_NETWORK_INFO[g_current_slot].icon.bpp = data[4] >> 4;
    DYNAMIC_NETWORK_INFO[g_current_slot].icon.isFile = true;
    COPY_FIELD(DYNAMIC_NETWORK_INFO[g_current_slot].icon.bitmap);
    print_icon_info();
    error = APDU_RESPONSE_OK;
end:
    return error;
}

/**
 * @brief Init the dynamic network icon with the 1st chunk.
 *
 * Analyze the 1st chunk, containing the icon size
 *
 * @param[in] data buffer received, skip payload length
 * @param[in] length of the buffer, reduced by the payload length
 * @return APDU Response code
 */
static uint16_t handle_first_icon_chunk(const uint8_t *data, uint8_t length) {
    uint16_t img_len = 0;
    uint16_t sw = APDU_RESPONSE_UNKNOWN;

    // Reset the structures
    explicit_bzero(&g_icon_payload, sizeof(g_icon_payload));
    explicit_bzero(g_network_icon[g_current_slot].bitmap, MAX_ICON_LEN);

    // Check the icon header
    sw = check_icon_header(data, length, &img_len);
    if (sw != APDU_RESPONSE_OK) {
        return sw;
    }
    if (img_len > MAX_ICON_LEN) {
        PRINTF("Icon size too large!\n");
        return APDU_RESPONSE_INSUFFICIENT_MEMORY;
    }
    g_icon_payload.expected_size = img_len;

    return APDU_RESPONSE_OK;
}

/**
 * @brief Handle icon data chunk.
 *
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
static uint16_t handle_next_icon_chunk(const uint8_t *data, uint8_t length) {
    if ((g_icon_payload.received_size + length) > g_icon_payload.expected_size) {
        PRINTF("Payload size mismatch!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    // Feed into payload
    memcpy(g_network_icon[g_current_slot].bitmap + g_icon_payload.received_size, data, length);
    g_icon_payload.received_size += length;

    return APDU_RESPONSE_OK;
}

/**
 * @brief Handle icon chunks.
 *
 * @param[in] p1 APDU parameter 1
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
static uint16_t handle_icon_chunks(uint8_t p1, const uint8_t *data, uint8_t length) {
    uint16_t sw = APDU_RESPONSE_UNKNOWN;
    uint8_t hash[CX_SHA256_SIZE] = {0};

    if (memcmp(g_network_icon[g_current_slot].hash, hash, CX_SHA256_SIZE) == 0) {
        PRINTF("Error: Icon hash not set!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }

    // Check the received chunk index
    if (p1 == P1_FIRST_CHUNK) {
        // Init the with the 1st chunk
        sw = handle_first_icon_chunk(data, length);
        if (sw != APDU_RESPONSE_OK) {
            return sw;
        }
    } else if (p1 != P1_FOLLOWING_CHUNK) {
        PRINTF("Error: Unexpected P2 (%u)!\n", p1);
        return APDU_RESPONSE_INVALID_P1_P2;
    }

    // Handle the payload
    sw = handle_next_icon_chunk(data, length);
    if (sw != APDU_RESPONSE_OK) {
        return sw;
    }
    if (g_icon_payload.received_size == g_icon_payload.expected_size) {
        // Everything has been received
        sw = parse_icon_buffer();
    }
    return sw;
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

/**
 * @brief Verify the payload signature
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] sig_ctx the signature context
 * @return whether it was successful
 */
static bool verify_signature(s_sig_ctx *sig_ctx) {
    uint8_t hash[INT256_LENGTH];
    cx_err_t error = CX_INTERNAL_ERROR;
    bool ret_code = false;

    CX_CHECK(
        cx_hash_no_throw((cx_hash_t *) &sig_ctx->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH));

#ifdef HAVE_LEDGER_PKI
    CX_CHECK(check_signature_with_pubkey("Dynamic Network",
                                         hash,
                                         sizeof(hash),
                                         LEDGER_SIGNATURE_PUBLIC_KEY,
                                         sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                         CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
                                         (uint8_t *) (sig_ctx->sig),
                                         sig_ctx->sig_size));
#else
    CX_CHECK(check_signature_with_pubkey("Dynamic Network",
                                         hash,
                                         sizeof(hash),
                                         LEDGER_SIGNATURE_PUBLIC_KEY,
                                         sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                         (uint8_t *) (sig_ctx->sig),
                                         sig_ctx->sig_size));
#endif

    ret_code = true;
end:
    return ret_code;
}

/**
 * @brief Print the registered network.
 *
 * Only for debug purpose.
 */
static void print_network_info(void) {
    char chain_str[sizeof(uint64_t) * 2 + 1] = {0};

    PRINTF("****************************************************************************\n");
    u64_to_string(DYNAMIC_NETWORK_INFO[g_current_slot].chain_id, chain_str, sizeof(chain_str));
    PRINTF("[NETWORK] - Registered in slot %d: %s (%s), for chain_id %s\n",
           g_current_slot,
           DYNAMIC_NETWORK_INFO[g_current_slot].name,
           DYNAMIC_NETWORK_INFO[g_current_slot].ticker,
           chain_str);
}

/**
 * @brief Returns the current network configuration.
 *
 * @return APDU length
 */
static uint16_t handle_get_config(void) {
    uint8_t chain_str[sizeof(uint64_t) * 2 + 1];
    uint32_t tx = 1;  // Init to '1' because there is at least the number of networks
    uint16_t nb_networks = 0;

    for (size_t i = 0; i < MAX_DYNAMIC_NETWORKS; i++) {
        if (DYNAMIC_NETWORK_INFO[i].chain_id != 0) {
            PRINTF("[NETWORK] - Found dynamic %s\n", DYNAMIC_NETWORK_INFO[i].name);
            // Convert chain_id
            explicit_bzero(chain_str, sizeof(chain_str));
            write_u64_be(chain_str, 0, DYNAMIC_NETWORK_INFO[i].chain_id);
            memmove(G_io_apdu_buffer + tx, chain_str, sizeof(uint64_t));
            tx += sizeof(uint64_t);
            nb_networks++;
        }
    }
    G_io_apdu_buffer[0] = nb_networks;

    return tx;
}

static bool handle_dyn_net_struct(const s_tlv_data *data, s_sig_ctx *context) {
    bool ret;

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
            ret = true;
#endif
            break;
        case TAG_DER_SIGNATURE:
            ret = parse_signature(data->value, data->length, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    if (ret && (data->tag != TAG_DER_SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    }
    return ret;
}

static bool handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    s_sig_ctx ctx = {0};

    // Set the current slot here, because the corresponding icon will be received
    // separately, after the network configuration, and should keep the same slot
    g_current_slot = (g_current_slot + 1) % MAX_DYNAMIC_NETWORKS;

    // Reset the structures
    explicit_bzero(&DYNAMIC_NETWORK_INFO[g_current_slot], sizeof(network_info_t));
#ifdef HAVE_NBGL
    explicit_bzero(&g_network_icon[g_current_slot], sizeof(network_icon_t));
#endif
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);
    if (!tlv_parse(payload, size, (f_tlv_data_handler) &handle_dyn_net_struct, &ctx) ||
        !verify_signature(&ctx)) {
        explicit_bzero(&DYNAMIC_NETWORK_INFO[g_current_slot], sizeof(network_info_t));
        return false;
    }
    print_network_info();
    return true;
}

/**
 * @brief Handle Network Configuration APDU.
 *
 * @param[in] p1 APDU parameter 1
 * @param[in] p2 APDU parameter 2
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @param[in] tx output length
 * @return APDU Response code
 */
uint16_t handleNetworkConfiguration(uint8_t p1,
                                    uint8_t p2,
                                    const uint8_t *data,
                                    uint8_t length,
                                    unsigned int *tx) {
    uint16_t sw = APDU_RESPONSE_UNKNOWN;

    switch (p2) {
        case P2_NETWORK_CONFIG:
            if (p1 != 0x00) {
                PRINTF("Error: Unexpected P1 (%u)!\n", p1);
                sw = APDU_RESPONSE_INVALID_P1_P2;
                break;
            }
            if (handle_tlv_payload(data, length)) {
                sw = APDU_RESPONSE_OK;
            } else {
                sw = APDU_RESPONSE_INVALID_DATA;
            }
            break;

        case P2_NETWORK_ICON:
#ifdef HAVE_NBGL
            sw = handle_icon_chunks(p1, data, length);
#else
            PRINTF("Warning: Network icon not supported!\n");
            sw = APDU_RESPONSE_OK;
#endif
            break;

        case P2_GET_INFO:
            if (p1 != 0x00) {
                PRINTF("Error: Unexpected P1 (%u)!\n", p1);
                sw = APDU_RESPONSE_INVALID_P1_P2;
                break;
            }
            *tx = handle_get_config();
            sw = APDU_RESPONSE_OK;
            break;
        default:
            sw = APDU_RESPONSE_INVALID_P1_P2;
            break;
    }

#ifdef HAVE_NBGL
    if ((sw != APDU_RESPONSE_OK) ||
        (g_icon_payload.received_size == g_icon_payload.expected_size)) {
        explicit_bzero(&g_icon_payload, sizeof(g_icon_payload));
    }
#endif

    return sw;
}
