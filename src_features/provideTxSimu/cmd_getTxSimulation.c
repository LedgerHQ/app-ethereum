#include <string.h>
#include <ctype.h>
#include "os_utils.h"
#include "os_pic.h"
#include "cmd_getTxSimulation.h"
#include "apdu_constants.h"
#include "hash_bytes.h"
#include "public_keys.h"
#ifdef HAVE_LEDGER_PKI
#include "os_pki.h"
#endif
#include "network.h"

#define DER_LONG_FORM_FLAG        0x80  // 8th bit set
#define DER_FIRST_BYTE_VALUE_MASK 0x7f

#define TYPE_TX_SIMULATION 0x09
#define STRUCT_VERSION     0x01

#define THRESHOLD_SCORE_WARNING   0x5556
#define THRESHOLD_SCORE_MALICIOUS 0xAAAB

#define P1_NORMAL 0x00
#define P1_DEMO   0x01

// clang-format off
enum {
    CATEGORY_RAW_SIGNING,
    CATEGORY_TRNSFER_SCAMS,
    CATEGORY_NB_MAX
};
// clang-format on

enum {
    TAG_STRUCTURE_TYPE = 0x01,
    TAG_STRUCTURE_VERSION = 0x02,
    TAG_CHAIN_ID = 0x23,
    TAG_TX_HASH = 0x27,
    TAG_W3C_NORMALIZED_RISK = 0x80,
    TAG_W3C_NORMALIZED_CATEGORY = 0x81,
    TAG_W3C_PROVIDER_MSG = 0x82,
    TAG_W3C_TINY_URL = 0x83,
    TAG_DER_SIGNATURE = 0x15,
};

// This enum needs to be ordered the same way as the TAGGs above !
enum {
    BIT_STRUCTURE_TYPE,
    BIT_STRUCTURE_VERSION,
    BIT_CHAIN_ID,
    BIT_TX_HASH,
    BIT_W3C_NORMALIZED_RISK,
    BIT_W3C_NORMALIZED_CATEGORY,
    BIT_W3C_PROVIDER_MSG,
    BIT_W3C_TINY_URL,
    BIT_DER_SIGNATURE,
};

// clang-format off
static const char *const TX_SIMULATION_CATEGORY[CATEGORY_NB_MAX] = {
    "Raw signing detected",   // CATEGORY_RAW_SIGNING
    "Transfer scam detected"  // CATEGORY_TRNSFER_SCAMS
};

static const char *const TX_SIMULATION_SCORE[SCORE_NB_MAX] = {
    "UNKNOWN (W3C Issue)",
    "BENIGN",
    "RISK (WARNING)",
    "THREAT (MALICIOUS)"
};
// clang-format on

// Signature context structure
typedef struct {
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
} s_sig_ctx;

// Global structure to store the tx simultion parameters
tx_simulation_t TX_SIMULATION = {0};

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
    CHECK_FIELD_VALUE("STRUCTURE_TYPE", data[0], TYPE_TX_SIMULATION);
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
    CHECK_FIELD_VALUE("STRUCTURE_VERSION", data[0], STRUCT_VERSION);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the TX_HASH value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_tx_hash(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_LENGTH("TX_HASH", field_len, HASH_SIZE);
    COPY_FIELD(TX_SIMULATION.tx_hash);
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

    TX_SIMULATION.chain_id = chain_id;
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the W3C_NORMALIZED_RISK value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_risk(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_LENGTH("W3C_NORMALIZED_RISK", field_len, sizeof(TX_SIMULATION.risk));
    TX_SIMULATION.risk = U2BE(data, 0);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Set the score associated to the risk.
 *
 */
static void set_score(void) {
    uint8_t hash[INT256_LENGTH] = {0};
    if ((TX_SIMULATION.chain_id == 0) || memcmp(TX_SIMULATION.tx_hash, hash, HASH_SIZE) == 0) {
        TX_SIMULATION.score = SCORE_UNKNOWN;
    } else if (TX_SIMULATION.risk < THRESHOLD_SCORE_WARNING) {
        TX_SIMULATION.score = SCORE_BENIGN;
    } else if (TX_SIMULATION.risk < THRESHOLD_SCORE_MALICIOUS) {
        TX_SIMULATION.score = SCORE_WARNING;
    } else {
        TX_SIMULATION.score = SCORE_MALICIOUS;
    }
}

/**
 * @brief Parse the W3C_NORMALIZED_CATEGORY value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_category(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_LENGTH("W3C_NNORMALIZED_CATEGORY", field_len, sizeof(TX_SIMULATION.category));
    TX_SIMULATION.category = data[0];
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
 * @brief Parse the W3C_PROVIDER_MSG value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_provider_msg(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_OVERFLOW("W3C_PROVIDER_MSG", TX_SIMULATION.provider_msg);
    // Check if the name is printable
    if (!check_name(data, field_len)) {
        PRINTF("W3C_PROVIDER_MSG is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(TX_SIMULATION.provider_msg);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the W3C_TINY_URL value.
 *
 * @param[in] data buffer received
 * @param[in] field_len Length of the field value
 * @return APDU Response code
 */
static uint16_t parse_tiny_url(const uint8_t *data, uint16_t field_len) {
    CHECK_FIELD_OVERFLOW("W3C_TINY_URL", TX_SIMULATION.tiny_url);
    // Check if the name is printable
    if (!check_name(data, field_len)) {
        PRINTF("W3C_TINY_URL is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(TX_SIMULATION.tiny_url);
    return APDU_RESPONSE_OK;
}

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

    CX_CHECK(check_signature_with_pubkey("Tx Simulation",
                                         hash,
                                         sizeof(hash),
                                         LEDGER_SIGNATURE_PUBLIC_KEY,
                                         sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                         CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
#endif
                                         (uint8_t *) (sig_ctx->sig),
                                         sig_ctx->sig_size));

    ret_code = true;
end:
    return ret_code;
}

/**
 * @brief Verify the received fields
 *
 * Check the mandatory fields are present
 *
 * @param[in] rcv_bit indicates received fields
 * @return whether it was successful
 */
static bool verify_fields(uint32_t rcv_bit) {
    uint32_t expected_fields;
    expected_fields = (1 << BIT_STRUCTURE_TYPE) | (1 << BIT_STRUCTURE_VERSION) |
                      (1 << BIT_CHAIN_ID) | (1 << BIT_TX_HASH) | (1 << BIT_W3C_NORMALIZED_RISK) |
                      (1 << BIT_W3C_NORMALIZED_CATEGORY) | (1 << BIT_W3C_TINY_URL) |
                      (1 << BIT_DER_SIGNATURE);

    if ((rcv_bit & expected_fields) != expected_fields) {
        return false;
    }

    return true;
}

/**
 * @brief Print the simulation parameters.
 *
 * Only for debug purpose.
 */
static void print_simulation_info(void) {
    char chain_str[sizeof(uint64_t) * 2 + 1] = {0};

    PRINTF("****************************************************************************\n");
    u64_to_string(TX_SIMULATION.chain_id, chain_str, sizeof(chain_str));
    PRINTF("[TX SIMU] - Retrieved TX simulation:\n");
    PRINTF("[TX SIMU] -    Hash: %.*h\n", HASH_SIZE, TX_SIMULATION.tx_hash);
    PRINTF("[TX SIMU] -    ChainID: %s\n", chain_str);
    PRINTF("[TX SIMU] -    Risk: %d (0x%x)\n", TX_SIMULATION.risk, TX_SIMULATION.risk);
    PRINTF("[TX SIMU] -    Risk score: %d -> %s\n", TX_SIMULATION.score, getTxSimuScoreStr());
    PRINTF("[TX SIMU] -    Category: %d (0x%x - %s)\n",
           TX_SIMULATION.category,
           TX_SIMULATION.category,
           getTxSimuCategoryStr());
    PRINTF("[TX SIMU] -    Provider Msg: %s\n", TX_SIMULATION.provider_msg);
    PRINTF("[TX SIMU] -    Tiny URL: %s\n", TX_SIMULATION.tiny_url);
}

/** Parse DER-encoded value
 *
 * Parses a DER-encoded value (up to 4 bytes long)
 * https://en.wikipedia.org/wiki/X.690
 *
 * @param[in] data the TLV payload buffer
 * @param[in] length the TLV payload length
 * @param[in,out] offset the payload offset
 * @param[out] value the parsed value
 * @return APDU Response code
 */
static uint16_t parse_der_value(const uint8_t *data,
                                uint8_t length,
                                uint32_t *offset,
                                uint32_t *value) {
    uint16_t sw = APDU_RESPONSE_INVALID_DATA;
    uint8_t data_length;
    uint8_t buf[4] = {0};

    if (value != NULL) {
        if (data[*offset] & DER_LONG_FORM_FLAG) {  // long form
            data_length = data[*offset] & DER_FIRST_BYTE_VALUE_MASK;
            *offset += 1;
            if ((*offset + data_length) > length) {
                PRINTF("TLV payload too small for DER encoded value\n");
            } else if (data_length > sizeof(buf) || data_length == 0) {
                PRINTF("Unexpectedly long DER-encoded value (%u bytes)\n", data_length);
            } else {
                memcpy(buf + (sizeof(buf) - data_length), &data[*offset], data_length);
                *value = U4BE(buf, 0);
                *offset += data_length;
                sw = APDU_RESPONSE_OK;
            }
        } else {  // short form
            *value = data[*offset];
            *offset += 1;
            sw = APDU_RESPONSE_OK;
        }
    }
    return sw;
}

/**
 * @brief Parse the TLV payload containing the TX Simulation parameters.
 *
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
static uint16_t parse_tlv(const uint8_t *data, uint8_t length) {
    uint32_t offset = 0;
    uint32_t field_tag = 0;
    uint32_t field_len = 0;
    uint16_t sw = APDU_RESPONSE_INTERNAL_ERROR;
    uint32_t tag_start_off;
    s_sig_ctx sig_ctx = {0};
    uint32_t rcv_bit = 0;

    // Reset the structures
    explicit_bzero(&TX_SIMULATION, sizeof(tx_simulation_t));
    // Initialize the hash context
    cx_sha256_init(&sig_ctx.hash_ctx);
    // handle TLV payload
    while (offset != length) {
        tag_start_off = offset;
        sw = parse_der_value(data, length, &offset, &field_tag);
        if (sw != APDU_RESPONSE_OK) {
            break;
        }
        sw = parse_der_value(data, length, &offset, &field_len);
        if (sw != APDU_RESPONSE_OK) {
            break;
        }
        if ((offset + field_len) > length) {
            PRINTF("Field length mismatch (%d/%d)!\n", (offset + field_len), length);
            sw = APDU_RESPONSE_INVALID_DATA;
            break;
        }
        switch (field_tag) {
            case TAG_STRUCTURE_TYPE:
                sw = parse_struct_type(data + offset, field_len);
                rcv_bit |= (1 << BIT_STRUCTURE_TYPE);
                break;
            case TAG_STRUCTURE_VERSION:
                sw = parse_struct_version(data + offset, field_len);
                rcv_bit |= (1 << BIT_STRUCTURE_VERSION);
                break;
            case TAG_CHAIN_ID:
                sw = parse_chain_id(data + offset, field_len);
                rcv_bit |= (1 << BIT_CHAIN_ID);
                break;
            case TAG_TX_HASH:
                sw = parse_tx_hash(data + offset, field_len);
                rcv_bit |= (1 << BIT_TX_HASH);
                break;
            case TAG_W3C_NORMALIZED_RISK:
                sw = parse_risk(data + offset, field_len);
                rcv_bit |= (1 << BIT_W3C_NORMALIZED_RISK);
                break;
            case TAG_W3C_NORMALIZED_CATEGORY:
                sw = parse_category(data + offset, field_len);
                rcv_bit |= (1 << BIT_W3C_NORMALIZED_CATEGORY);
                break;
            case TAG_W3C_PROVIDER_MSG:
                sw = parse_provider_msg(data + offset, field_len);
                rcv_bit |= (1 << BIT_W3C_PROVIDER_MSG);
                break;
            case TAG_W3C_TINY_URL:
                sw = parse_tiny_url(data + offset, field_len);
                rcv_bit |= (1 << BIT_W3C_TINY_URL);
                break;
            case TAG_DER_SIGNATURE:
                sw = parse_signature(data + offset, field_len, &sig_ctx);
                rcv_bit |= (1 << BIT_DER_SIGNATURE);
                break;
            default:
                PRINTF("Skipping unknown tag: %d\n", field_tag);
                sw = APDU_RESPONSE_OK;
                break;
        }
        if (sw != APDU_RESPONSE_OK) {
            break;
        }
        offset += field_len;
        if (field_tag != TAG_DER_SIGNATURE) {  // the signature wasn't computed on itself
            hash_nbytes(data + tag_start_off,
                        (offset - tag_start_off),
                        (cx_hash_t *) &sig_ctx.hash_ctx);
        }
    }
    if (sw == APDU_RESPONSE_OK) {
        if (verify_fields(rcv_bit) == false) {
            PRINTF("Missing fields!\n");
            sw = APDU_RESPONSE_INVALID_DATA;
        } else if (verify_signature(&sig_ctx) == false) {
            PRINTF("Signature verification failed!\n");
            sw = APDU_RESPONSE_INVALID_DATA;
        }
    }
    if (sw == APDU_RESPONSE_OK) {
        set_score();
        print_simulation_info();
    } else {
        // Reset the structure
        explicit_bzero(&TX_SIMULATION, sizeof(tx_simulation_t));
    }
    return sw;
}

/**
 * @brief Handle Tx Simulation APDU.
 *
 * @param[in] p1 APDU parameter 1
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
uint16_t handleTxSimulation(uint8_t p1, const uint8_t *data, uint8_t length) {
    uint16_t sw = APDU_RESPONSE_UNKNOWN;

    if (!N_storage.web3checks) {
        PRINTF("Error: Web3 checks are disabled!\n");
        return APDU_RESPONSE_CMD_CODE_NOT_SUPPORTED;
    }
    if ((p1 != P1_NORMAL) && (p1 != P1_DEMO)) {
        PRINTF("Error: Unexpected P1 (%u)!\n", p1);
        return APDU_RESPONSE_INVALID_P1_P2;
    }
    sw = parse_tlv(data, length);
    if (sw != APDU_RESPONSE_OK) {
        return sw;
    }

#ifdef HAVE_NBGL
    if (p1 == P1_DEMO) {
        // Display the TX Simulation parameters
        ui_display_tx_simulation(true);
    }
#endif

    return APDU_RESPONSE_OK;
}

/**
 * @brief Clear the TX Simulation parameters.
 *
 */
void clearTxSimulation(void) {
    explicit_bzero(&TX_SIMULATION, sizeof(tx_simulation_t));
}

/**
 * @brief Check the TX vs Simulation parameters (CHAIN_ID, TX_HASH).
 *
 */
void checkTxSimulationParams(void) {
    uint64_t chain_id = get_tx_chain_id();

    if (TX_SIMULATION.chain_id != chain_id) {
        PRINTF("[TX SIMU] Chain_ID mismatch: %u != %u\n", TX_SIMULATION.chain_id, chain_id);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.score = SCORE_UNKNOWN;
        return;
    }
    if (memcmp(TX_SIMULATION.tx_hash, tmpCtx.transactionContext.hash, HASH_SIZE) != 0) {
        PRINTF("[TX SIMU] TX_HASH mismatch: %.*h != %.*h\n",
               HASH_SIZE,
               TX_SIMULATION.tx_hash,
               INT256_LENGTH,
               tmpCtx.transactionContext.hash);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.score = SCORE_UNKNOWN;
    }
}

/**
 * @brief Retrieve the TX Simulation score string.
 *
 * @return risk score as a string
 */
const char *getTxSimuScoreStr(void) {
    if (TX_SIMULATION.score < SCORE_NB_MAX) {
        return PIC(TX_SIMULATION_SCORE[TX_SIMULATION.score]);
    }
    return NULL;
}

/**
 * @brief Retrieve the TX Simulation category string.
 *
 * @return category string
 */
const char *getTxSimuCategoryStr(void) {
    if (TX_SIMULATION.category < CATEGORY_NB_MAX) {
        return PIC(TX_SIMULATION_CATEGORY[TX_SIMULATION.category]);
    }
    return NULL;
}
