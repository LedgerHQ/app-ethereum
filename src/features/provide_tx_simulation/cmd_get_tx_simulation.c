#ifdef HAVE_TRANSACTION_CHECKS

#include "cmd_get_tx_simulation.h"
#include "apdu_constants.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "getPublicKey.h"
#include "tlv_library.h"
#include "tlv_apdu.h"
#include "utils.h"
#include "nbgl_use_case.h"
#include "os_pki.h"
#include "network.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "signature.h"

#define TYPE_TX_SIMULATION 0x09
#define STRUCT_VERSION     0x01

enum {
    CATEGORY_OTHERS = 0x01,
    CATEGORY_ADDRESS = 0x02,
    CATEGORY_DAPP = 0x03,
    CATEGORY_LOSING_OPERATION = 0x04,
};

typedef struct {
    tx_simulation_t *simu;
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
    TLV_reception_t received_tags;
} s_tx_simu_ctx;

// Global structure to store the tx simultion parameters
tx_simulation_t TX_SIMULATION = {0};

/**
 * @brief Parse the STRUCTURE_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_struct_type(const tlv_data_t *data, s_tx_simu_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_type(data, TYPE_TX_SIMULATION);
}

/**
 * @brief Parse the STRUCTURE_VERSION value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_struct_version(const tlv_data_t *data, s_tx_simu_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, STRUCT_VERSION);
}

/**
 * @brief Parse the TX_HASH value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_tx_hash(const tlv_data_t *data, s_tx_simu_ctx *context) {
    return tlv_get_hash(data, (char *) context->simu->tx_hash);
}

/**
 * @brief Parse the DOMAIN_HASH value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_domain_hash(const tlv_data_t *data, s_tx_simu_ctx *context) {
    return tlv_get_hash(data, (char *) context->simu->domain_hash);
}

/**
 * @brief Parse the ADDRESS value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_address(const tlv_data_t *data, s_tx_simu_ctx *context) {
    return tlv_get_address(data, (uint8_t *) context->simu->address, true);
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_chain_id(const tlv_data_t *data, s_tx_simu_ctx *context) {
    return tlv_get_chain_id(data, &context->simu->chain_id);
}

/**
 * @brief Parse the TX_CHECKS_NORMALIZED_RISK value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_risk(const tlv_data_t *data, s_tx_simu_ctx *context) {
    uint8_t value = 0;
    if (!tlv_get_uint8(data, &value, 0, RISK_MALICIOUS - 1)) {
        PRINTF("TX_CHECKS_NORMALIZED_RISK: error\n");
        return false;
    }
    context->simu->risk = value + 1;  // Because 0 is "unknown"
    return true;
}

/**
 * @brief Parse the TX_CHECKS_NORMALIZED_CATEGORY value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_category(const tlv_data_t *data, s_tx_simu_ctx *context) {
    if (!tlv_get_uint8(data, &context->simu->category, 0, UINT8_MAX)) {
        PRINTF("TX_CHECKS_NORMALIZED_CATEGORY: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the TX_CHECKS_SIMU_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_type(const tlv_data_t *data, s_tx_simu_ctx *context) {
    uint8_t value = 0;
    if (!tlv_get_uint8(data, &value, 0, SIMU_TYPE_PERSONAL_MESSAGE - 1)) {
        PRINTF("TX_CHECKS_SIMU_TYPE: error\n");
        return false;
    }
    context->simu->type = value + 1;  // Because 0 is "unknown"
    return true;
}

/**
 * @brief Parse the TX_CHECKS_PROVIDER_MSG value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_provider_msg(const tlv_data_t *data, s_tx_simu_ctx *context) {
    if (!tlv_get_printable_string(data,
                                  (char *) context->simu->provider_msg,
                                  0,
                                  sizeof(context->simu->provider_msg))) {
        PRINTF("TX_CHECKS_PROVIDER_MSG: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the TX_CHECKS_TINY_URL value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_tiny_url(const tlv_data_t *data, s_tx_simu_ctx *context) {
    if (!tlv_get_printable_string(data,
                                  (char *) context->simu->tiny_url,
                                  0,
                                  sizeof(context->simu->tiny_url))) {
        PRINTF("TX_CHECKS_TINY_URL: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data the tlv data
 * @param[in] context TX Simu context
 * @return whether the handling was successful
 */
static bool parse_signature(const tlv_data_t *data, s_tx_simu_ctx *context) {
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

// Define TLV tags for TX Simulation
#define TX_SIMULATION_TAGS(X)                                                      \
    X(0x01, TAG_STRUCTURE_TYPE, parse_struct_type, ENFORCE_UNIQUE_TAG)             \
    X(0x02, TAG_STRUCTURE_VERSION, parse_struct_version, ENFORCE_UNIQUE_TAG)       \
    X(0x22, TAG_ADDRESS, parse_address, ENFORCE_UNIQUE_TAG)                        \
    X(0x23, TAG_CHAIN_ID, parse_chain_id, ENFORCE_UNIQUE_TAG)                      \
    X(0x27, TAG_TX_HASH, parse_tx_hash, ENFORCE_UNIQUE_TAG)                        \
    X(0x28, TAG_DOMAIN_HASH, parse_domain_hash, ENFORCE_UNIQUE_TAG)                \
    X(0x80, TAG_TX_CHECKS_NORMALIZED_RISK, parse_risk, ENFORCE_UNIQUE_TAG)         \
    X(0x81, TAG_TX_CHECKS_NORMALIZED_CATEGORY, parse_category, ENFORCE_UNIQUE_TAG) \
    X(0x82, TAG_TX_CHECKS_PROVIDER_MSG, parse_provider_msg, ENFORCE_UNIQUE_TAG)    \
    X(0x83, TAG_TX_CHECKS_TINY_URL, parse_tiny_url, ENFORCE_UNIQUE_TAG)            \
    X(0x84, TAG_TX_CHECKS_SIMU_TYPE, parse_type, ENFORCE_UNIQUE_TAG)               \
    X(0x15, TAG_DER_SIGNATURE, parse_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration of common handler
static bool tx_simu_common_handler(const tlv_data_t *data, s_tx_simu_ctx *context);

// Generate TLV parser for TX Simulation
DEFINE_TLV_PARSER(TX_SIMULATION_TAGS, &tx_simu_common_handler, tx_simulation_tlv_parser)

/**
 * @brief Common handler called for all tags to hash them (except signature).
 *
 * @param[in] data data to handle
 * @param[out] context struct context
 * @return whether the handling was successful
 */
static bool tx_simu_common_handler(const tlv_data_t *data, s_tx_simu_ctx *context) {
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
 * @param[in] context TX Simu context
 * @return whether it was successful
 */
static bool verify_signature(const s_tx_simu_ctx *context) {
    uint8_t hash[INT256_LENGTH] = {0};
    uint8_t key_usage = 0;
    size_t trusted_name_len = 0;
    uint8_t trusted_name[CERTIFICATE_TRUSTED_NAME_MAXLEN] = {0};
    cx_ecfp_384_public_key_t public_key = {0};

    if (finalize_hash((cx_hash_t *) &context->hash_ctx, hash, sizeof(hash)) != true) {
        PRINTF("Could not finalize struct hash!\n");
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_TX_SIMU_SIGNER,
                                    (uint8_t *) context->sig,
                                    context->sig_size) != true) {
        return false;
    }

    // Partner name is retrieved from the certificate
    if (os_pki_get_info(&key_usage, trusted_name, &trusted_name_len, &public_key) != 0) {
        PRINTF("Failed to get the certificate info\n");
        return false;
    }
    // Last byte is the NULL terminator
    memmove((void *) context->simu->partner, trusted_name, PARTNER_SIZE - 1);
    return true;
}

/**
 * @brief Verify the received fields
 *
 * Check the mandatory fields are present
 *
 * @param[in] context TX Simu context
 * @return whether it was successful
 */
static bool verify_fields(const s_tx_simu_ctx *context) {
    // Common mandatory fields for all types
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                 TAG_STRUCTURE_TYPE,
                                 TAG_STRUCTURE_VERSION,
                                 TAG_TX_HASH,
                                 TAG_ADDRESS,
                                 TAG_TX_CHECKS_NORMALIZED_RISK,
                                 TAG_TX_CHECKS_NORMALIZED_CATEGORY,
                                 TAG_TX_CHECKS_TINY_URL,
                                 TAG_TX_CHECKS_SIMU_TYPE,
                                 TAG_DER_SIGNATURE)) {
        return false;
    }

    // Type-specific fields
    if (context->simu->type == SIMU_TYPE_TRANSACTION) {
        if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_CHAIN_ID)) {
            return false;
        }
    }
    if (context->simu->type == SIMU_TYPE_TYPED_DATA) {
        if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_DOMAIN_HASH)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Print the simulation parameters.
 *
 * @param[in] context TX Simu context
 * Only for debug purpose.
 */
static void print_simulation_info(s_tx_simu_ctx *context) {
    PRINTF("****************************************************************************\n");
    PRINTF("[TX SIMU] - Retrieved TX simulation:\n");
    PRINTF("[TX SIMU] -    Partner: %s\n", context->simu->partner);
    PRINTF("[TX SIMU] -    Hash: %.*h\n", HASH_SIZE, context->simu->tx_hash);
    PRINTF("[TX SIMU] -    Address: %.*h\n", ADDRESS_LENGTH, context->simu->address);
    if (context->simu->chain_id != 0) {
        PRINTF("[TX SIMU] -    ChainID: %llu\n", context->simu->chain_id);
    }
    PRINTF("[TX SIMU] -    Risk: %d -> %s\n", context->simu->risk, get_tx_simulation_risk_str());
    PRINTF("[TX SIMU] -    Category: %d -> %s\n",
           context->simu->category,
           get_tx_simulation_category_str());
    PRINTF("[TX SIMU] -    Provider Msg: %s\n", context->simu->provider_msg);
    PRINTF("[TX SIMU] -    Tiny URL: %s\n", context->simu->tiny_url);
}

/**
 * @brief Verify the struct
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context TX Simu context
 * @return whether it was successful
 */
static bool verify_simulation_struct(const s_tx_simu_ctx *context) {
    if (!verify_fields(context)) {
        PRINTF("Error: Missing mandatory fields in TX Simulation descriptor!\n");
        return false;
    }

    if (!verify_signature(context)) {
        PRINTF("Error: Signature verification failed for TX Simulation descriptor!\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the TLV payload containing the TX Simulation parameters.
 *
 * @param[in] buf TLV buffer received
 * @return whether the TLV payload was handled successfully or not
 */
static bool handle_tlv_payload(const buffer_t *buf) {
    bool ret = false;
    s_tx_simu_ctx ctx = {0};

    ctx.simu = &TX_SIMULATION;
    // Reset the structures
    explicit_bzero(&TX_SIMULATION, sizeof(TX_SIMULATION));
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    ret = tx_simulation_tlv_parser(buf, &ctx, &ctx.received_tags);
    if (ret) {
        ret = verify_simulation_struct(&ctx);
    }
    if (ret) {
        if (strlen(ctx.simu->partner) == 0) {
            // Set a default value for partner
            snprintf((char *) ctx.simu->partner, sizeof(ctx.simu->partner), "Transaction Checks");
        }
        print_simulation_info(&ctx);
    } else {
        clear_tx_simulation();
    }
    return ret;
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
            io_seproxyhal_send_status(SWO_SUCCESS, 1, false, true);
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
    uint16_t sw = SWO_NOT_SUPPORTED_ERROR_NO_INFO;

    switch (p1) {
        case 0x00:
            // TX Simulation data
            if (!N_storage.tx_check_enable) {
                PRINTF("Error: TX_CHECKS_ check is disabled!\n");
                sw = SWO_COMMAND_CODE_NOT_SUPPORTED;
                break;
            }
            if (!tlv_from_apdu(p2 == P1_FIRST_CHUNK, length, data, &handle_tlv_payload)) {
                sw = SWO_INCORRECT_DATA;
            } else {
                sw = SWO_SUCCESS;
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
            sw = SWO_WRONG_P1_P2;
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
static bool check_tx_simulation_hash(void) {
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
static bool check_tx_simulation_from_address(void) {
    uint8_t msg_sender[ADDRESS_LENGTH] = {0};
    if (get_public_key(msg_sender, sizeof(msg_sender)) != SWO_SUCCESS) {
        PRINTF("[TX SIMU] Unable to get the public key!\n");
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    if (memcmp(TX_SIMULATION.address, msg_sender, ADDRESS_LENGTH) != 0) {
        PRINTF("[TX SIMU] FROM address mismatch: %.*h != %.*h\n",
               ADDRESS_LENGTH,
               TX_SIMULATION.address,
               ADDRESS_LENGTH,
               msg_sender);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    return true;
}

/**
 * @brief Check the CHAIN_ID vs Simulation payload.
 *
 * @return whether it was successful
 */
static bool check_tx_simulation_chain_id(void) {
    uint64_t chain_id = get_tx_chain_id();
    // Check Chain_ID in case of a standard transaction (No EIP191, No EIP712)
    if ((appState == APP_STATE_SIGNING_TX) && (TX_SIMULATION.chain_id != chain_id)) {
        PRINTF("[TX SIMU] Chain_ID mismatch: %llu != %llu\n", TX_SIMULATION.chain_id, chain_id);
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        TX_SIMULATION.risk = RISK_UNKNOWN;
        return false;
    }
    return true;
}

/**
 * @brief Check the validity of the Simulation parameters.
 *
 * @return whether it was successful
 */
static bool check_tx_simulation_validity(void) {
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
    return true;
}

/**
 * @brief Check the TX vs Simulation parameters (CHAIN_ID, TX_HASH).
 *
 * @return whether it was successful
 */
static bool check_tx_simulation_params(void) {
    if (!N_storage.tx_check_enable) {
        // Transaction Checks disabled
        return true;
    }
    if (check_tx_simulation_validity() == false) {
        return false;
    }
    if (check_tx_simulation_chain_id() == false) {
        return false;
    }
    if (check_tx_simulation_from_address() == false) {
        return false;
    }
    if (check_tx_simulation_hash() == false) {
        return false;
    }
    return true;
}

/**
 * @brief Configure the warning predefined set for the NBGL review flows.
 *
 */
void set_tx_simulation_warning(void) {
    if (!N_storage.tx_check_enable) {
        // Transaction Checks disabled
        return;
    }
    // Transaction Checks enabled => Verify parameters of the Transaction
    check_tx_simulation_params();
    switch (TX_SIMULATION.risk) {
        case RISK_UNKNOWN:
            warning.predefinedSet |= SET_BIT(W3C_ISSUE_WARN);
            break;
        case RISK_BENIGN:
            warning.predefinedSet |= SET_BIT(W3C_NO_THREAT_WARN);
            break;
        case RISK_WARNING:
            warning.predefinedSet |= SET_BIT(W3C_RISK_DETECTED_WARN);
            break;
        case RISK_MALICIOUS:
            warning.predefinedSet |= SET_BIT(W3C_THREAT_DETECTED_WARN);
            break;
        default:
            break;
    }
    warning.reportProvider = PIC(TX_SIMULATION.partner);
    warning.providerMessage = get_tx_simulation_category_str();
    warning.reportUrl = PIC(TX_SIMULATION.tiny_url);
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
