#ifdef HAVE_TRANSACTION_CHECKS

#include "cmd_get_tx_simulation.h"
#include "apdu_constants.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "getPublicKey.h"
#include "tlv.h"
#include "tlv_apdu.h"
#include "utils.h"
#include "nbgl_use_case.h"
#include "os_pki.h"
#include "network.h"
#include "ui_callbacks.h"

#define TYPE_TX_SIMULATION 0x09
#define STRUCT_VERSION     0x01

enum {
    CATEGORY_OTHERS = 0x01,
    CATEGORY_ADDRESS = 0x02,
    CATEGORY_DAPP = 0x03,
    CATEGORY_LOSING_OPERATION = 0x04,
};

enum {
    TAG_STRUCTURE_TYPE = 0x01,
    TAG_STRUCTURE_VERSION = 0x02,
    TAG_ADDRESS = 0x22,
    TAG_CHAIN_ID = 0x23,
    TAG_TX_HASH = 0x27,
    TAG_DOMAIN_HASH = 0x28,
    TAG_TX_CHECKS_NORMALIZED_RISK = 0x80,
    TAG_TX_CHECKS_NORMALIZED_CATEGORY = 0x81,
    TAG_TX_CHECKS_PROVIDER_MSG = 0x82,
    TAG_TX_CHECKS_TINY_URL = 0x83,
    TAG_TX_CHECKS_SIMU_TYPE = 0x84,
    TAG_DER_SIGNATURE = 0x15,
};

enum {
    BIT_STRUCTURE_TYPE,
    BIT_STRUCTURE_VERSION,
    BIT_ADDRESS,
    BIT_CHAIN_ID,
    BIT_TX_HASH,
    BIT_DOMAIN_HASH,
    BIT_TX_CHECKS_NORMALIZED_RISK,
    BIT_TX_CHECKS_NORMALIZED_CATEGORY,
    BIT_TX_CHECKS_PROVIDER_MSG,
    BIT_TX_CHECKS_TINY_URL,
    BIT_TX_CHECKS_SIMU_TYPE,
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

// Macro to check the field value
#define CHECK_EMPTY_BUFFER(tag, field, len)    \
    do {                                       \
        if (memcmp(field, empty, len) == 0) {  \
            PRINTF("%s Zero buffer!\n", tag);  \
            return APDU_RESPONSE_INVALID_DATA; \
        }                                      \
    } while (0)

// Macro to copy the field
#define COPY_FIELD(field, data)                             \
    do {                                                    \
        memmove((void *) field, data->value, data->length); \
    } while (0)

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
    uint8_t empty[HASH_SIZE] = {0};
    CHECK_FIELD_LENGTH("TX_HASH", data->length, HASH_SIZE);
    CHECK_EMPTY_BUFFER("TX_HASH", data->value, data->length);
    COPY_FIELD(context->simu->tx_hash, data);
    context->rcv_flags |= SET_BIT(BIT_TX_HASH);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the DOMAIN_HASH value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_domain_hash(const s_tlv_data *data, s_tx_simu_ctx *context) {
    uint8_t empty[HASH_SIZE] = {0};
    CHECK_FIELD_LENGTH("DOMAIN_HASH", data->length, HASH_SIZE);
    CHECK_EMPTY_BUFFER("DOMAIN_HASH", data->value, data->length);
    COPY_FIELD(context->simu->domain_hash, data);
    context->rcv_flags |= SET_BIT(BIT_DOMAIN_HASH);
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
    uint8_t empty[ADDRESS_LENGTH] = {0};
    CHECK_FIELD_LENGTH("ADDRESS", data->length, ADDRESS_LENGTH);
    CHECK_EMPTY_BUFFER("ADDRESS", data->value, data->length);
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
    if ((chain_id > max_range) || (chain_id == 0)) {
        PRINTF("Unsupported chain ID: %u\n", chain_id);
        return APDU_RESPONSE_INVALID_DATA;
    }

    context->simu->chain_id = chain_id;
    context->rcv_flags |= SET_BIT(BIT_CHAIN_ID);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the TX_CHECKS_NORMALIZED_RISK value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_risk(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("TX_CHECKS_NORMALIZED_RISK", data->length, sizeof(context->simu->risk));
    if (data->value[0] >= RISK_MALICIOUS) {
        PRINTF("TX_CHECKS_NORMALIZED_RISK out of range: %d\n", data->value[0]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    context->simu->risk = data->value[0] + 1;  // Because 0 is "unknown"
    context->rcv_flags |= SET_BIT(BIT_TX_CHECKS_NORMALIZED_RISK);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the TX_CHECKS_NORMALIZED_CATEGORY value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_category(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("TX_CHECKS_NORMALIZED_CATEGORY",
                       data->length,
                       sizeof(context->simu->category));
    context->simu->category = data->value[0];
    context->rcv_flags |= SET_BIT(BIT_TX_CHECKS_NORMALIZED_CATEGORY);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the TX_CHECKS_SIMU_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_type(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_LENGTH("TX_CHECKS_SIMU_TYPE", data->length, sizeof(context->simu->type));
    if (data->value[0] >= SIMU_TYPE_PERSONAL_MESSAGE) {
        PRINTF("TX_CHECKS_SIMU_TYPE out of range: %d\n", data->value[0]);
        return APDU_RESPONSE_INVALID_DATA;
    }
    context->simu->type = data->value[0] + 1;  // Because 0 is "unknown"
    context->rcv_flags |= SET_BIT(BIT_TX_CHECKS_SIMU_TYPE);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the TX_CHECKS_PROVIDER_MSG value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_provider_msg(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_OVERFLOW("TX_CHECKS_PROVIDER_MSG", context->simu->provider_msg, data->length);
    // Check if the name is printable
    if (!check_name(data->value, data->length)) {
        PRINTF("TX_CHECKS_PROVIDER_MSG is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(context->simu->provider_msg, data);
    context->rcv_flags |= SET_BIT(BIT_TX_CHECKS_PROVIDER_MSG);
    return APDU_RESPONSE_OK;
}

/**
 * @brief Parse the TX_CHECKS_TINY_URL value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return APDU Response code
 */
static uint16_t parse_tiny_url(const s_tlv_data *data, s_tx_simu_ctx *context) {
    CHECK_FIELD_OVERFLOW("TX_CHECKS_TINY_URL", context->simu->tiny_url, data->length);
    // Check if the name is printable
    if (!check_name(data->value, data->length)) {
        PRINTF("TX_CHECKS_TINY_URL is not printable!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    COPY_FIELD(context->simu->tiny_url, data);
    context->rcv_flags |= SET_BIT(BIT_TX_CHECKS_TINY_URL);
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
                                         CERTIFICATE_PUBLIC_KEY_USAGE_TX_SIMU_SIGNER,
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
 * @param[in] context TX Simu context
 * @return whether it was successful
 */
static bool verify_fields(s_tx_simu_ctx *context) {
    uint32_t expected_fields;

    expected_fields = (1 << BIT_STRUCTURE_TYPE) | (1 << BIT_STRUCTURE_VERSION) |
                      (1 << BIT_TX_HASH) | (1 << BIT_ADDRESS) |
                      (1 << BIT_TX_CHECKS_NORMALIZED_RISK) |
                      (1 << BIT_TX_CHECKS_NORMALIZED_CATEGORY) | (1 << BIT_TX_CHECKS_TINY_URL) |
                      (1 << BIT_TX_CHECKS_SIMU_TYPE) | (1 << BIT_DER_SIGNATURE);

    if (context->simu->type == SIMU_TYPE_TRANSACTION) {
        expected_fields |= (1 << BIT_CHAIN_ID);
    }
    if (context->simu->type == SIMU_TYPE_TYPED_DATA) {
        expected_fields |= (1 << BIT_DOMAIN_HASH);
    }

    return ((context->rcv_flags & expected_fields) == expected_fields);
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
    PRINTF("[TX SIMU] - Retrieved TX simulation:\n");
    PRINTF("[TX SIMU] -    Partner: %s\n", context->simu->partner);
    PRINTF("[TX SIMU] -    Hash: %.*h\n", HASH_SIZE, context->simu->tx_hash);
    PRINTF("[TX SIMU] -    Address: %.*h\n", ADDRESS_LENGTH, context->simu->addr);
    if (context->simu->chain_id != 0) {
        u64_to_string(context->simu->chain_id, chain_str, sizeof(chain_str));
        PRINTF("[TX SIMU] -    ChainID: %s\n", chain_str);
    }
    PRINTF("[TX SIMU] -    Risk: %d -> %s\n", context->simu->risk, get_tx_simulation_risk_str());
    PRINTF("[TX SIMU] -    Category: %d -> %s\n",
           context->simu->category,
           get_tx_simulation_category_str());
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
        case TAG_DOMAIN_HASH:
            sw = parse_domain_hash(data, context);
            break;
        case TAG_TX_CHECKS_NORMALIZED_RISK:
            sw = parse_risk(data, context);
            break;
        case TAG_TX_CHECKS_NORMALIZED_CATEGORY:
            sw = parse_category(data, context);
            break;
        case TAG_TX_CHECKS_PROVIDER_MSG:
            sw = parse_provider_msg(data, context);
            break;
        case TAG_TX_CHECKS_TINY_URL:
            sw = parse_tiny_url(data, context);
            break;
        case TAG_TX_CHECKS_SIMU_TYPE:
            sw = parse_type(data, context);
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
 * @param[in] payload buffer received
 * @param[in] size of the buffer
 * @return whether the TLV payload was handled successfully or not
 */
static bool handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    bool parsing_ret;
    s_tx_simu_ctx ctx = {0};

    ctx.simu = &TX_SIMULATION;
    // Reset the structures
    explicit_bzero(&TX_SIMULATION, sizeof(TX_SIMULATION));
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    parsing_ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_tx_simu_tlv, &ctx);
    if (!parsing_ret || !verify_fields(&ctx) || !verify_signature(&ctx)) {
        explicit_bzero(&TX_SIMULATION, sizeof(TX_SIMULATION));
        explicit_bzero(&ctx, sizeof(s_tx_simu_ctx));
        return false;
    }
    if (strlen(ctx.simu->partner) == 0) {
        // Set a default value for partner
        snprintf((char *) ctx.simu->partner, sizeof(ctx.simu->partner), "Transaction Checks");
    }
    print_simulation_info(&ctx);
    return true;
}

/**
 * @brief Handle Tx Simulation Opt-In.
 *
 * @param[in] response_expected indicates if a response is expected
 */
void handle_tx_simulation_opt_in(bool response_expected) {
    if (N_storage.tx_check_opt_in) {
        // TX_CHECKS_ Checks already Opt-In
        PRINTF("TX_CHECKS_ Checks already Opt-in!\n");
        if (response_expected) {
            // just respond the current state and return to idle screen
            G_io_apdu_buffer[0] = N_storage.tx_check_enable;
            io_seproxyhal_send_status(APDU_RESPONSE_OK, 1, false, true);
        }
        return;
    }
    ui_tx_simulation_opt_in(response_expected);
}

/**
 * @brief Handle Tx Simulation APDU.
 *
 * @param[in] p1 APDU parameter 1 (indicates Data payload or Opt-In request)
 * @param[in] p2 APDU parameter 2 (indicates if the payload is the first chunk)
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
uint16_t handle_tx_simulation(uint8_t p1,
                              uint8_t p2,
                              const uint8_t *data,
                              uint8_t length,
                              unsigned int *flags) {
    uint16_t sw = APDU_RESPONSE_INTERNAL_ERROR;

    switch (p1) {
        case 0x00:
            // TX Simulation data
            if (!N_storage.tx_check_enable) {
                PRINTF("Error: TX_CHECKS_ check is disabled!\n");
                sw = APDU_RESPONSE_CMD_CODE_NOT_SUPPORTED;
                break;
            }
            if (!tlv_from_apdu(p2 == P1_FIRST_CHUNK, length, data, &handle_tlv_payload)) {
                sw = APDU_RESPONSE_INVALID_DATA;
            } else {
                sw = APDU_RESPONSE_OK;
            }
            break;
        case 0x01:
            // TX Simulation Opt-In
            handle_tx_simulation_opt_in(true);
            *flags |= IO_ASYNCH_REPLY;
            sw = APDU_NO_RESPONSE;
            break;
        default:
            PRINTF("Error: Unexpected P1 (%u)!\n", p1);
            sw = APDU_RESPONSE_INVALID_P1_P2;
            break;
    }
    return sw;
}

/**
 * @brief Clear the TX Simulation parameters.
 *
 */
void clear_tx_simulation(void) {
    explicit_bzero(&TX_SIMULATION, sizeof(TX_SIMULATION));
}

/**
 * @brief Check the TX HASH vs Simulation payload.
 *
 * @return whether it was successful
 */
bool check_tx_simulation_hash(void) {
    uint8_t *hash = NULL;
    uint8_t *hash2 = NULL;

    if (!N_storage.tx_check_enable) {
        // Transaction Checks disabled
        return true;
    }
    switch (appState) {
        case APP_STATE_SIGNING_TX:
            hash = tmpCtx.transactionContext.hash;
            break;
        case APP_STATE_SIGNING_MESSAGE:
            hash = tmpCtx.messageSigningContext.hash;
            break;
        case APP_STATE_SIGNING_EIP712:
            hash = tmpCtx.messageSigningContext712.messageHash;
            hash2 = tmpCtx.messageSigningContext712.domainHash;
            break;
        default:
            PRINTF("[TX SIMU] Invalid app State %d!\n", appState);
            TX_SIMULATION.risk = RISK_UNKNOWN;
            return false;
    }
    if (memcmp(TX_SIMULATION.tx_hash, hash, HASH_SIZE) != 0) {
        PRINTF("[TX SIMU] TX_HASH mismatch: %.*h != %.*h\n",
               HASH_SIZE,
               TX_SIMULATION.tx_hash,
               HASH_SIZE,
               hash);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    if ((hash2 != NULL) && (memcmp(TX_SIMULATION.domain_hash, hash2, HASH_SIZE)) != 0) {
        PRINTF("[TX SIMU] DOMAIN_HASH mismatch: %.*h != %.*h\n",
               HASH_SIZE,
               TX_SIMULATION.domain_hash,
               HASH_SIZE,
               hash);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    return true;
}

/**
 * @brief Check the FROM_ADDRESS vs Simulation payload.
 *
 * @return whether it was successful
 */
bool check_tx_simulation_from_address(void) {
    uint8_t msg_sender[ADDRESS_LENGTH] = {0};
    if (get_public_key(msg_sender, sizeof(msg_sender)) != APDU_RESPONSE_OK) {
        PRINTF("[TX SIMU] Unable to get the public key!\n");
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    if (memcmp(TX_SIMULATION.addr, msg_sender, ADDRESS_LENGTH) != 0) {
        PRINTF("[TX SIMU] FROM addr mismatch: %.*h != %.*h\n",
               ADDRESS_LENGTH,
               TX_SIMULATION.addr,
               ADDRESS_LENGTH,
               msg_sender);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    return true;
}

/**
 * @brief Check the TX vs Simulation parameters (CHAIN_ID, TX_HASH).
 *
 * @param[in] checkTxHash flag to check the TX_HASH
 * @param[in] checkFromAddr flag to check the FROM address
 * @return whether it was successful
 */
static bool check_tx_simulation_params(bool checkTxHash, bool checkFromAddr) {
    uint64_t chain_id = get_tx_chain_id();

    if (!N_storage.tx_check_enable) {
        // Transaction Checks disabled
        return true;
    }
    switch (TX_SIMULATION.type) {
        case SIMU_TYPE_TRANSACTION:
            if (appState != APP_STATE_SIGNING_TX) {
                PRINTF("[TX SIMU] Simulation type inconsistent!\n");
                TX_SIMULATION.risk = RISK_UNKNOWN;
                return false;
            }
            break;
        case SIMU_TYPE_PERSONAL_MESSAGE:
            if (appState != APP_STATE_SIGNING_MESSAGE) {
                PRINTF("[TX SIMU] Simulation type inconsistent!\n");
                TX_SIMULATION.risk = RISK_UNKNOWN;
                return false;
            }
            break;
        case SIMU_TYPE_TYPED_DATA:
            if (appState != APP_STATE_SIGNING_EIP712) {
                PRINTF("[TX SIMU] Simulation type inconsistent!\n");
                TX_SIMULATION.risk = RISK_UNKNOWN;
                return false;
            }
            break;
        default:
            // No simulation data
            PRINTF("[TX SIMU] Simulation type is not set\n");
            PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
            TX_SIMULATION.risk = RISK_UNKNOWN;
            return false;
    }
    if (TX_SIMULATION.risk == RISK_UNKNOWN) {
        // No simulation data
        PRINTF("[TX SIMU] Simulation risk is not set\n");
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        return false;
    }
    // Check Chain_ID in case of a standard transaction (No EIP191, No EIP712)
    if ((appState == APP_STATE_SIGNING_TX) && (TX_SIMULATION.chain_id != chain_id)) {
        PRINTF("[TX SIMU] Chain_ID mismatch: %u != %u\n", TX_SIMULATION.chain_id, chain_id);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    if (checkFromAddr) {
        if (check_tx_simulation_from_address() == false) {
            return false;
        }
    }
    if (checkTxHash) {
        if (check_tx_simulation_hash() == false) {
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
void set_tx_simulation_warning(nbgl_warning_t *p_warning, bool checkTxHash, bool checkFromAddr) {
    if (!N_storage.tx_check_enable) {
        // Transaction Checks disabled
        return;
    }
    // Transaction Checks enabled => Verify parameters of the Transaction
    check_tx_simulation_params(checkTxHash, checkFromAddr);
    switch (TX_SIMULATION.risk) {
        case RISK_UNKNOWN:
            p_warning->predefinedSet |= SET_BIT(W3C_ISSUE_WARN);
            break;
        case RISK_BENIGN:
            p_warning->predefinedSet |= SET_BIT(W3C_NO_THREAT_WARN);
            break;
        case RISK_WARNING:
            p_warning->predefinedSet |= SET_BIT(W3C_RISK_DETECTED_WARN);
            break;
        case RISK_MALICIOUS:
            p_warning->predefinedSet |= SET_BIT(W3C_THREAT_DETECTED_WARN);
            break;
        default:
            break;
    }
    p_warning->reportProvider = PIC(TX_SIMULATION.partner);
    p_warning->providerMessage = get_tx_simulation_category_str();
    p_warning->reportUrl = PIC(TX_SIMULATION.tiny_url);
}

/**
 * @brief Retrieve the TX Simulation risk string.
 *
 * @return risk as a string
 */
const char *get_tx_simulation_risk_str(void) {
    switch (TX_SIMULATION.risk) {
        case RISK_UNKNOWN:
            return "UNKNOWN (Transaction Check Issue)";
        case RISK_BENIGN:
            return "BENIGN";
        case RISK_WARNING:
            return "RISK (WARNING)";
        case RISK_MALICIOUS:
            return "THREAT (MALICIOUS)";
        default:
            break;
    }
    return "INVALID";
}

/**
 * @brief Retrieve the TX Simulation category string.
 *
 * @return category string
 */
const char *get_tx_simulation_category_str(void) {
    // Unknown category string
    switch (TX_SIMULATION.risk) {
        case RISK_UNKNOWN:
        case RISK_BENIGN:
            break;
        case RISK_WARNING:
            switch (TX_SIMULATION.category) {
                case CATEGORY_ADDRESS:
                    return "This transaction involves a suspicious address. "
                           "It might not be safe to continue.";
                case CATEGORY_DAPP:
                    return "This transaction involves a suspicious dApp. "
                           "It might not be safe to continue.";
                case CATEGORY_LOSING_OPERATION:
                    return "This transaction could end in a loss. "
                           "Check transaction details carefully before signing.";
                default:
                    return "This transaction might be malicious. It might not be safe to continue. "
                           "Tap the QR code icon for more details.";
                    break;
            }
            break;
        case RISK_MALICIOUS:
            switch (TX_SIMULATION.category) {
                case CATEGORY_ADDRESS:
                    return "This transaction involves a malicious address. "
                           "Your assets will most likely be stolen.";
                case CATEGORY_DAPP:
                    return "This dApp is linked to a scammer. "
                           "Your assets will most likely be stolen.";
                default:
                    return "This request is malicious. Your assets will most likely be stolen. "
                           "View full report for details.";
            }
            break;
    }
    return "Unknown";
}

#endif  // HAVE_TRANSACTION_CHECKS
