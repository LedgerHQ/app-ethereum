#ifdef HAVE_WEB3_CHECKS

#include <string.h>
#include <ctype.h>
#include "os_utils.h"
#include "os_pic.h"
#include "cmd_getTxSimulation.h"
#include "apdu_constants.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "feature_signTx.h"
#include "tlv.h"
#include "utils.h"
#include "nbgl_use_case.h"
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

// clang-format off
enum {
    CATEGORY_BENIGN = 0x00,
    CATEGORY_SIGNATURE_FARMING = 0x01,
    CATEGORY_APPROVAL_FARMING = 0x02,
    CATEGORY_OTHER = 0x03,
    CATEGORY_SET_APPROVAL_FOR_ALL = 0x04,
    CATEGORY_TRANSFER_FARMING = 0x05,
    CATEGORY_TRANSFER_FROM_FARMING = 0x06,
    CATEGORY_RAW_ETHER_TRANSFER = 0x07,
    CATEGORY_SEAPORT_FARMING = 0x08,
    CATEGORY_BLUR_FARMING = 0x09,
    CATEGORY_PERMIT_FARMING = 0x0A,
    CATEGORY_USER_MISTAKE = 0x0B,
    CATEGORY_MALICIOUS_TOKEN = 0x0C,
    CATEGORY_ONCHAIN_INFECTION = 0x0D,
    CATEGORY_GAS_FARMING = 0x0E,
    CATEGORY_DELEGATECALL_EXECUTION = 0x0F,
    CATEGORY_EXPLOIT_CONTRACT_CREATION = 0x10,
    CATEGORY_EMPTY = 0xFF
};

enum {
    TAG_STRUCTURE_TYPE = 0x01,
    TAG_STRUCTURE_VERSION = 0x02,
    TAG_ADDRESS = 0x22,
    TAG_CHAIN_ID = 0x23,
    TAG_TX_HASH = 0x27,
    TAG_W3C_NORMALIZED_RISK = 0x80,
    TAG_W3C_NORMALIZED_CATEGORY = 0x81,
    TAG_W3C_PROVIDER_MSG = 0x82,
    TAG_W3C_TINY_URL = 0x83,
    TAG_DER_SIGNATURE = 0x15,
};
// clang-format on

// This enum needs to be ordered the same way as the TAGGs above !
enum {
    BIT_STRUCTURE_TYPE,
    BIT_STRUCTURE_VERSION,
    BIT_ADDRESS,
    BIT_CHAIN_ID,
    BIT_TX_HASH,
    BIT_W3C_NORMALIZED_RISK,
    BIT_W3C_NORMALIZED_CATEGORY,
    BIT_W3C_PROVIDER_MSG,
    BIT_W3C_TINY_URL,
    BIT_DER_SIGNATURE,
};

typedef struct {
    tx_simulation_t *simu;
    uint8_t sig_size;
    uint8_t *sig;
    cx_sha256_t hash_ctx;
    uint32_t rcv_flags;
} s_tx_simu_ctx;

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
#define CHECK_FIELD_OVERFLOW(tag, field, len)         \
    do {                                              \
        if (len >= sizeof(field)) {                   \
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
#define COPY_FIELD(field, data)                             \
    do {                                                    \
        memmove((void *) field, data->value, data->length); \
    } while (0)

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
 * @brief Parse the STRUCTURE_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_struct_type(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_TYPE", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_TYPE", data->value[0], TYPE_TX_SIMULATION);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_TYPE);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the STRUCTURE_VERSION value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_struct_version(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_VERSION", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_VERSION", data->value[0], STRUCT_VERSION);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_VERSION);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the TX_HASH value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_tx_hash(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("TX_HASH", data->length, HASH_SIZE);
    COPY_FIELD(context->simu->tx_hash, data);
    context->rcv_flags |= SET_BIT(BIT_TX_HASH);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the ADDRESS value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_address(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("ADDRESS", data->length, ADDRESS_LENGTH);
    COPY_FIELD(context->simu->addr, data);
    context->rcv_flags |= SET_BIT(BIT_ADDRESS);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_chain_id(const s_tlv_data *data, s_tx_simu_ctx *context) {
    uint64_t chain_id;
    uint64_t max_range;

    CHECK_FIELD_LENGTH("CHAIN_ID", data->length, sizeof(uint64_t));
    // Check if the chain ID is supported
    // https://github.com/ethereum/EIPs/blob/master/EIPS/eip-2294.md
    max_range = 0x7FFFFFFFFFFFFFDB;
    chain_id = u64_from_BE(data->value, data->length);
    // Check if the chain_id is supported
    if ((chain_id > max_range) || ((appState == APP_STATE_SIGNING_TX) && (chain_id == 0))) {
        PRINTF("Unsupported chain ID: %u\n", chain_id);
        return APDU_RESPONSE_INVALID_DATA;
    }

    context->simu->chain_id = chain_id;
    context->rcv_flags |= SET_BIT(BIT_CHAIN_ID);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the W3C_NORMALIZED_RISK value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_risk(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("W3C_NORMALIZED_RISK", data->length, sizeof(context->simu->risk));
    context->simu->risk = U2BE(data->value, 0);
    context->rcv_flags |= SET_BIT(BIT_W3C_NORMALIZED_RISK);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the W3C_NORMALIZED_CATEGORY value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_category(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("W3C_NNORMALIZED_CATEGORY", data->length, sizeof(context->simu->category));
    context->simu->category = data->value[0];
    context->rcv_flags |= SET_BIT(BIT_W3C_NORMALIZED_CATEGORY);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the W3C_PROVIDER_MSG value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_provider_msg(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_OVERFLOW("W3C_PROVIDER_MSG", context->simu->provider_msg, data->length);
    // Check if the name is printable
    if (!check_name(data->value, data->length)) {
        PRINTF("W3C_PROVIDER_MSG is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(context->simu->provider_msg, data);
    context->rcv_flags |= SET_BIT(BIT_W3C_PROVIDER_MSG);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the W3C_TINY_URL value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_tiny_url(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_OVERFLOW("W3C_TINY_URL", context->simu->tiny_url, data->length);
    // Check if the name is printable
    if (!check_name(data->value, data->length)) {
        PRINTF("W3C_TINY_URL is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(context->simu->tiny_url, data);
    context->rcv_flags |= SET_BIT(BIT_W3C_TINY_URL);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_signature(const s_tlv_data *data, s_tx_simu_ctx *context) {
    context->sig_size = data->length;
    context->sig = (uint8_t *) data->value;
    context->rcv_flags |= SET_BIT(BIT_DER_SIGNATURE);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Verify the payload signature
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context TX Simu context
 * @return whether it was successful
 */
static bool verify_signature(s_tx_simu_ctx *context) {
    uint8_t hash[INT256_LENGTH];
    cx_err_t error = CX_INTERNAL_ERROR;
    bool ret_code = false;

    CX_CHECK(
        cx_hash_no_throw((cx_hash_t *) &context->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH));

    CX_CHECK(check_signature_with_pubkey("Tx Simulation",
                                         hash,
                                         sizeof(hash),
                                         NULL,
                                         0,
#ifdef HAVE_LEDGER_PKI
                                         // TODO: change once SDK has the enum value for this
                                         0x0a,
#endif
                                         (uint8_t *) (context->sig),
                                         context->sig_size));

    // Partner name is retrieved from the certificate
    uint8_t key_usage = 0;
    size_t trusted_name_len = 0;
    uint8_t trusted_name[CERTIFICATE_TRUSTED_NAME_MAXLEN] = {0};
    cx_ecfp_384_public_key_t public_key = {0};
    if (os_pki_get_info(&key_usage, trusted_name, &trusted_name_len, &public_key) != 0) {
        PRINTF("Failed to get the certificate info\n");
        goto end;
    }
    explicit_bzero((void *) context->simu->partner, PARTNER_SIZE);
    // Last byte is the NULL terminator
    memmove((void *) context->simu->partner, trusted_name, PARTNER_SIZE - 1);
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
                      (1 << BIT_CHAIN_ID) | (1 << BIT_TX_HASH) | (1 << BIT_ADDRESS) |
                      (1 << BIT_W3C_NORMALIZED_RISK) | (1 << BIT_W3C_NORMALIZED_CATEGORY) |
                      (1 << BIT_W3C_TINY_URL) | (1 << BIT_DER_SIGNATURE);

    return ((rcv_bit & expected_fields) == expected_fields);
}

/**
 * @brief Set the score associated to the risk.
 *
 * @param[in] context TX Simu context
 */
static void set_score(s_tx_simu_ctx *context) {
    uint8_t hash[INT256_LENGTH] = {0};
    if (((appState == APP_STATE_SIGNING_TX) && (context->simu->chain_id == 0)) ||
        memcmp(context->simu->tx_hash, hash, HASH_SIZE) == 0) {
        context->simu->score = SCORE_UNKNOWN;
    } else if (context->simu->risk < THRESHOLD_SCORE_WARNING) {
        context->simu->score = SCORE_BENIGN;
    } else if (context->simu->risk < THRESHOLD_SCORE_MALICIOUS) {
        context->simu->score = SCORE_WARNING;
    } else {
        context->simu->score = SCORE_MALICIOUS;
    }
}

/**
 * @brief Print the simulation parameters.
 *
 * @param[in] context TX Simu context
 * Only for debug purpose.
 */
static void print_simulation_info(s_tx_simu_ctx *context) {
    char chain_str[sizeof(uint64_t) * 2 + 1] = {0};

    PRINTF("****************************************************************************\n");
    u64_to_string(context->simu->chain_id, chain_str, sizeof(chain_str));
    PRINTF("[TX SIMU] - Retrieved TX simulation:\n");
    PRINTF("[TX SIMU] -    Partner: %s\n", context->simu->partner);
    PRINTF("[TX SIMU] -    Hash: %.*h\n", HASH_SIZE, context->simu->tx_hash);
    PRINTF("[TX SIMU] -    Address: %.*h\n", ADDRESS_LENGTH, context->simu->addr);
    PRINTF("[TX SIMU] -    ChainID: %s\n", chain_str);
    PRINTF("[TX SIMU] -    Risk: %d (0x%x)\n", context->simu->risk, context->simu->risk);
    PRINTF("[TX SIMU] -    Risk score: %d -> %s\n", context->simu->score, getTxSimuScoreStr());
    PRINTF("[TX SIMU] -    Category: %d (0x%x - %s)\n",
           context->simu->category,
           context->simu->category,
           getTxSimuCategoryStr());
    PRINTF("[TX SIMU] -    Provider Msg: %s\n", context->simu->provider_msg);
    PRINTF("[TX SIMU] -    Tiny URL: %s\n", context->simu->tiny_url);
}

/**
 * @brief Parse the received TLV.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static bool handle_tx_simu_tlv(const s_tlv_data *data, s_tx_simu_ctx *context) {
    uint16_t sw = APDU_RESPONSE_INTERNAL_ERROR;

    switch (data->tag) {
        case TAG_STRUCTURE_TYPE:
            sw = parse_struct_type(data, context);
            break;
        case TAG_STRUCTURE_VERSION:
            sw = parse_struct_version(data, context);
            break;
        case TAG_CHAIN_ID:
            sw = parse_chain_id(data, context);
            break;
        case TAG_ADDRESS:
            sw = parse_address(data, context);
            break;
        case TAG_TX_HASH:
            sw = parse_tx_hash(data, context);
            break;
        case TAG_W3C_NORMALIZED_RISK:
            sw = parse_risk(data, context);
            break;
        case TAG_W3C_NORMALIZED_CATEGORY:
            sw = parse_category(data, context);
            break;
        case TAG_W3C_PROVIDER_MSG:
            sw = parse_provider_msg(data, context);
            break;
        case TAG_W3C_TINY_URL:
            sw = parse_tiny_url(data, context);
            break;
        case TAG_DER_SIGNATURE:
            sw = parse_signature(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            sw = APDU_RESPONSE_OK;
            break;
    }
    if ((sw == APDU_RESPONSE_OK) && (data->tag != TAG_DER_SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    }
    return (sw == APDU_RESPONSE_OK);
}

/**
 * @brief Parse the TLV payload containing the TX Simulation parameters.
 *
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
static uint16_t handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    s_tx_simu_ctx ctx = {0};

    ctx.simu = &TX_SIMULATION;
    // Reset the structures
    explicit_bzero(&TX_SIMULATION, sizeof(tx_simulation_t));
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    if (!tlv_parse(payload, size, (f_tlv_data_handler) &handle_tx_simu_tlv, &ctx) ||
        !verify_fields(ctx.rcv_flags) || !verify_signature(&ctx)) {
        explicit_bzero(&TX_SIMULATION, sizeof(tx_simulation_t));
        explicit_bzero(&ctx, sizeof(s_tx_simu_ctx));
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (strlen(ctx.simu->partner) == 0) {
        // Set a default value for partner
        snprintf((char *) ctx.simu->partner, sizeof(ctx.simu->partner), "Web3 Checks");
    }
    set_score(&ctx);
    print_simulation_info(&ctx);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Handle Tx Simulation APDU.
 *
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
uint16_t handleTxSimulation(const uint8_t *data, uint8_t length) {
    if (!N_storage.web3checks) {
        PRINTF("Error: Web3 checks are disabled!\n");
        return APDU_RESPONSE_CMD_CODE_NOT_SUPPORTED;
    }
    return handle_tlv_payload(data, length);
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
 * @param[in] checkTxHash flag to check the TX_HASH
 * @param[in] checkFromAddr flag to check the FROM address
 * @return whether it was successful
 */
bool checkTxSimulationParams(bool checkTxHash, bool checkFromAddr) {
    uint8_t msg_sender[ADDRESS_LENGTH] = {0};
    uint64_t chain_id = get_tx_chain_id();
    uint8_t *hash = NULL;

    if ((!N_storage.web3checks) || (TX_SIMULATION.score == SCORE_BENIGN)) {
        // W3Checks disabled or benign => No need to check the parameters
        return true;
    }

    if (TX_SIMULATION.chain_id != chain_id) {
        PRINTF("[TX SIMU] Chain_ID mismatch: %u != %u\n", TX_SIMULATION.chain_id, chain_id);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.score = SCORE_UNKNOWN;
        return false;
    }
    if (checkFromAddr) {
        if (get_public_key(msg_sender, sizeof(msg_sender)) != APDU_RESPONSE_OK) {
            PRINTF("[TX SIMU] Unable to get the public key!\n");
            PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
            TX_SIMULATION.score = SCORE_UNKNOWN;
            return false;
        }
        if (memcmp(TX_SIMULATION.addr, msg_sender, ADDRESS_LENGTH) != 0) {
            PRINTF("[TX SIMU] FROM addr mismatch: %.*h != %.*h\n",
                   ADDRESS_LENGTH,
                   TX_SIMULATION.addr,
                   ADDRESS_LENGTH,
                   msg_sender);
            PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
            TX_SIMULATION.score = SCORE_UNKNOWN;
            return false;
        }
    }
    if (checkTxHash) {
        switch (appState) {
            case APP_STATE_SIGNING_TX:
                hash = tmpCtx.transactionContext.hash;
                break;
            case APP_STATE_SIGNING_MESSAGE:
                hash = tmpCtx.messageSigningContext.hash;
                break;
            case APP_STATE_SIGNING_EIP712:
                hash = tmpCtx.messageSigningContext712.messageHash;
                break;
            default:
                PRINTF("[TX SIMU] Invalid app State %d!\n", appState);
                TX_SIMULATION.score = SCORE_UNKNOWN;
                return false;
        }
        if (memcmp(TX_SIMULATION.tx_hash, hash, HASH_SIZE) != 0) {
            PRINTF("[TX SIMU] TX_HASH mismatch: %.*h != %.*h\n",
                   HASH_SIZE,
                   TX_SIMULATION.tx_hash,
                   HASH_SIZE,
                   hash);
            PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
            TX_SIMULATION.score = SCORE_UNKNOWN;
            return false;
        }
    }
    return true;
}

/**
 * @brief Configure the warning predefined set for the NBGL review flows.
 *
 * @param[in] p_warning Warning structure for NBGL review flows
 * @param[in] checkTxHash flag to check the TX_HASH
 * @param[in] checkFromAddr flag to check the FROM address
 */
void setTxSimuWarning(nbgl_warning_t *p_warning, bool checkTxHash, bool checkFromAddr) {
    if ((N_storage.web3checks) && (TX_SIMULATION.score != SCORE_BENIGN)) {
        // W3Checks enabled => Verify parameters of the Transaction
        checkTxSimulationParams(checkTxHash, checkFromAddr);
        switch (TX_SIMULATION.score) {
            case SCORE_UNKNOWN:
                p_warning->predefinedSet |= SET_BIT(W3C_ISSUE_WARN);
                break;
            case SCORE_WARNING:
                p_warning->predefinedSet |= SET_BIT(W3C_LOSING_SWAP_WARN);
                break;
            case SCORE_MALICIOUS:
                p_warning->predefinedSet |= SET_BIT(W3C_THREAT_DETECTED_WARN);
                break;
            default:
                break;
        }
        p_warning->reportProvider = PIC(TX_SIMULATION.partner);
        p_warning->providerMessage = PIC(TX_SIMULATION.provider_msg);
        p_warning->reportUrl = PIC(TX_SIMULATION.tiny_url);
    }
}

/**
 * @brief Retrieve the TX Simulation score string.
 *
 * @return risk score as a string
 */
const char *getTxSimuScoreStr(void) {
    switch (TX_SIMULATION.score) {
        case SCORE_UNKNOWN:
            return "UNKNOWN (W3C Issue)";
        case SCORE_BENIGN:
            return "BENIGN";
        case SCORE_WARNING:
            return "RISK (WARNING)";
        case SCORE_MALICIOUS:
            return "THREAT (MALICIOUS)";
        default:
            break;
    }
    return "Unknown";
}

/**
 * @brief Retrieve the TX Simulation category string.
 *
 * @return category string
 */
const char *getTxSimuCategoryStr(void) {
    switch (TX_SIMULATION.category) {
        case CATEGORY_BENIGN:
            return "No signs of malicious activity.";
        case CATEGORY_SIGNATURE_FARMING:
            return "Collecting signatures for misuse.";
        case CATEGORY_APPROVAL_FARMING:
            return "Obtaining excessive user approvals.";
        case CATEGORY_OTHER:
            return "Uncategorized suspicious activity.";
        case CATEGORY_SET_APPROVAL_FOR_ALL:
            return "Granting broad asset permissions.";
        case CATEGORY_TRANSFER_FARMING:
            return "Tricking users to transfer assets.";
        case CATEGORY_TRANSFER_FROM_FARMING:
            return "Exploiting pre-approved transfers.";
        case CATEGORY_RAW_ETHER_TRANSFER:
            return "Direct Ether transfers for fraud.";
        case CATEGORY_SEAPORT_FARMING:
            return "Exploiting Seaport protocol actions";
        case CATEGORY_BLUR_FARMING:
            return "Abusing Blur platform mechanisms.";
        case CATEGORY_PERMIT_FARMING:
            return "Misusing ERC-20 permits for assets.";
        case CATEGORY_USER_MISTAKE:
            return "Errors made by the end-user.";
        case CATEGORY_MALICIOUS_TOKEN:
            return "Tokens designed to harm users.";
        case CATEGORY_ONCHAIN_INFECTION:
            return "Malware spreading via smart contracts.";
        case CATEGORY_GAS_FARMING:
            return "Extracting excessive gas fees.";
        case CATEGORY_DELEGATECALL_EXECUTION:
            return "Malicious code via delegatecall.";
        case CATEGORY_EXPLOIT_CONTRACT_CREATION:
            return "Creating contracts for exploitation.";
        case CATEGORY_EMPTY:
            return "Empty.";
        default:
            break;
    }
    return "Unknown";
}

#endif  // HAVE_WEB3_CHECKS
