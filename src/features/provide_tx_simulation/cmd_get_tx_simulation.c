#ifdef HAVE_TRANSACTION_CHECKS

#include "cmd_get_tx_simulation.h"
#include "apdu_constants.h"
#include "get_public_key.h"
#include "tlv_apdu.h"
#include "tlv_utils.h"
#include "utils.h"
#include "nbgl_use_case.h"
#include "network.h"
#include "ui_callbacks.h"
#include "ui_nbgl.h"
#include "tlv_use_case_transaction_check.h"

// Ethereum-specific struct with EVM-fixed-size address field
typedef struct tx_simu_s {
    bool received;
    uint64_t chain_id;
    uint8_t tx_hash[TRANSACTION_CHECK_HASH_SIZE];
    uint8_t domain_hash[TRANSACTION_CHECK_HASH_SIZE];
    char provider_msg[TRANSACTION_CHECK_MSG_SIZE + 1];
    char tiny_url[TRANSACTION_CHECK_URL_SIZE + 1];
    uint8_t address[ADDRESS_LENGTH];
    char partner[TRANSACTION_CHECK_PARTNER_SIZE];
    transaction_check_risk_t risk;
    transaction_check_type_t type;
    transaction_check_category_t category;
} tx_simulation_t;

// Global structure to store the tx simulation parameters
static tx_simulation_t G_transaction_check_info = {0};

/**
 * @brief Print the simulation parameters (debug only).
 */
static void print_simulation_info(void) {
    PRINTF("****************************************************************************\n");
    PRINTF("[TX SIMU] - Retrieved TX simulation:\n");
    PRINTF("[TX SIMU] -    Partner: %s\n", G_transaction_check_info.partner);
    PRINTF("[TX SIMU] -    Hash: %.*h\n",
           TRANSACTION_CHECK_HASH_SIZE,
           G_transaction_check_info.tx_hash);
    PRINTF("[TX SIMU] -    Address: %.*h\n", ADDRESS_LENGTH, G_transaction_check_info.address);
    if (G_transaction_check_info.chain_id != 0) {
        PRINTF("[TX SIMU] -    ChainID: %llu\n", G_transaction_check_info.chain_id);
    }
    PRINTF("[TX SIMU] -    Risk: %d -> %s\n",
           G_transaction_check_info.risk,
           get_tx_simulation_risk_str());
    PRINTF("[TX SIMU] -    Category: %d -> %s\n",
           G_transaction_check_info.category,
           get_tx_simulation_category_str());
    PRINTF("[TX SIMU] -    Provider Msg: %s\n", G_transaction_check_info.provider_msg);
    PRINTF("[TX SIMU] -    Tiny URL: %s\n", G_transaction_check_info.tiny_url);
}

/**
 * @brief Parse the TLV payload containing the TX Simulation parameters.
 *
 * Delegates TLV parsing and signature verification to the SDK Transaction Check use case,
 * then copies the result into the Ethereum-specific G_transaction_check_info struct.
 *
 * @param[in] buf TLV buffer received
 * @return whether the TLV payload was handled successfully or not
 */
static bool handle_tlv_payload(const buffer_t *buf) {
    tlv_transaction_check_out_t out = {0};

    // Reset the global simulation struct
    explicit_bzero(&G_transaction_check_info, sizeof(G_transaction_check_info));

    // Delegate to the use case for TLV parsing + signature verification
    tlv_transaction_check_status_t status = tlv_use_case_transaction_check(buf, &out);
    if (status != TLV_TRANSACTION_CHECK_SUCCESS) {
        PRINTF("[TX SIMU] Transaction Check failed with status %d\n", status);
        clear_tx_simulation();
        return false;
    }

    // Reject ADDITIONAL_DATA (not supported by Ethereum app)
    if (out.additional_data_received) {
        PRINTF("[TX SIMU] Unexpected ADDITIONAL_DATA tag\n");
        clear_tx_simulation();
        return false;
    }

    // Ethereum address is exactly ADDRESS_LENGTH (20 bytes)
    if (out.address.size != ADDRESS_LENGTH) {
        PRINTF("[TX SIMU] Invalid address size: %d\n", out.address.size);
        clear_tx_simulation();
        return false;
    }

    // Type-specific required fields
    if (out.type == TRANSACTION_CHECK_TYPE_TRANSACTION) {
        if (!out.chain_id_received) {
            PRINTF("[TX SIMU] Missing required chain_id for TRANSACTION_CHECK_TYPE_TRANSACTION\n");
            clear_tx_simulation();
            return false;
        }
    } else if (out.type == TRANSACTION_CHECK_TYPE_TYPED_DATA) {
        if (!out.domain_hash_received) {
            PRINTF(
                "[TX SIMU] Missing required domain_hash for TRANSACTION_CHECK_TYPE_TYPED_DATA\n");
            clear_tx_simulation();
            return false;
        }
    }

    // Copy validated data into long-lived global
    G_transaction_check_info.chain_id = out.chain_id;
    memmove(G_transaction_check_info.tx_hash, out.tx_hash.ptr, TRANSACTION_CHECK_HASH_SIZE);
    if (out.domain_hash_received) {
        memmove(G_transaction_check_info.domain_hash,
                out.domain_hash.ptr,
                TRANSACTION_CHECK_HASH_SIZE);
    }
    memmove(G_transaction_check_info.provider_msg,
            out.provider_msg,
            sizeof(G_transaction_check_info.provider_msg));
    memmove(G_transaction_check_info.tiny_url,
            out.tiny_url,
            sizeof(G_transaction_check_info.tiny_url));
    memmove(G_transaction_check_info.address, out.address.ptr, ADDRESS_LENGTH);
    memmove(G_transaction_check_info.partner,
            out.partner,
            sizeof(G_transaction_check_info.partner));
    G_transaction_check_info.risk = out.risk;
    G_transaction_check_info.type = out.type;
    G_transaction_check_info.category = out.category;
    G_transaction_check_info.received = true;

    print_simulation_info();
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
            G_io_tx_buffer[0] = N_storage.tx_check_enable;
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
    explicit_bzero(&G_transaction_check_info, sizeof(G_transaction_check_info));
}

/**
 * @brief Check the TX HASH vs Simulation payload.
 *
 * @return whether it was successful
 */
static bool check_tx_simulation_hash(void) {
    uint8_t *hash = NULL;
    uint8_t *hash2 = NULL;

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
            return false;
    }
    if (memcmp(G_transaction_check_info.tx_hash, hash, TRANSACTION_CHECK_HASH_SIZE) != 0) {
        PRINTF("[TX SIMU] TX_HASH mismatch: %.*h != %.*h\n",
               TRANSACTION_CHECK_HASH_SIZE,
               G_transaction_check_info.tx_hash,
               TRANSACTION_CHECK_HASH_SIZE,
               hash);
        return false;
    }
    if ((hash2 != NULL) &&
        (memcmp(G_transaction_check_info.domain_hash, hash2, TRANSACTION_CHECK_HASH_SIZE)) != 0) {
        PRINTF("[TX SIMU] DOMAIN_HASH mismatch: %.*h != %.*h\n",
               TRANSACTION_CHECK_HASH_SIZE,
               G_transaction_check_info.domain_hash,
               TRANSACTION_CHECK_HASH_SIZE,
               hash);
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
        return false;
    }
    if (memcmp(G_transaction_check_info.address, msg_sender, ADDRESS_LENGTH) != 0) {
        PRINTF("[TX SIMU] FROM address mismatch: %.*h != %.*h\n",
               ADDRESS_LENGTH,
               G_transaction_check_info.address,
               ADDRESS_LENGTH,
               msg_sender);
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
    if ((appState == APP_STATE_SIGNING_TX) && (G_transaction_check_info.chain_id != chain_id)) {
        PRINTF("[TX SIMU] Chain_ID mismatch: %llu != %llu\n",
               G_transaction_check_info.chain_id,
               chain_id);
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
    switch (G_transaction_check_info.type) {
        case TRANSACTION_CHECK_TYPE_TRANSACTION:
            if (appState != APP_STATE_SIGNING_TX) {
                PRINTF("[TX SIMU] Simulation type inconsistent!\n");
                return false;
            }
            break;
        case TRANSACTION_CHECK_TYPE_TYPED_DATA:
            if (appState != APP_STATE_SIGNING_EIP712) {
                PRINTF("[TX SIMU] Simulation type inconsistent!\n");
                return false;
            }
            break;
        default:
            // No simulation data
            PRINTF("[TX SIMU] Simulation type is not set\n");
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
    if (!G_transaction_check_info.received) {
        return false;
    }
    if (!check_tx_simulation_validity()) {
        return false;
    }
    if (!check_tx_simulation_chain_id()) {
        return false;
    }
    if (!check_tx_simulation_from_address()) {
        return false;
    }
    if (!check_tx_simulation_hash()) {
        return false;
    }
    return true;
}

/**
 * @brief Configure the warning predefined set for the NBGL review flows.
 *
 * Performs Ethereum-specific validation (hash, address, chain_id) and then delegates
 * the warning configuration to the SDK's transaction_check_set_warning().
 */
void set_tx_simulation_warning(void) {
    if (!N_storage.tx_check_enable) {
        // Transaction Checks disabled
        PRINTF("Transaction Checks are disabled, skipping\n");
        return;
    }
    // Transaction Checks enabled => Verify parameters of the Transaction
    if (!check_tx_simulation_params()) {
        // Fallback
        PRINTF("[TX SIMU] Force Score to UNKNOWN\n");
        warning.predefinedSet |= SET_BIT(W3C_ISSUE_WARN);
    } else {
        switch (G_transaction_check_info.risk) {
            case TRANSACTION_CHECK_RISK_BENIGN:
                warning.predefinedSet |= SET_BIT(W3C_NO_THREAT_WARN);
                break;
            case TRANSACTION_CHECK_RISK_WARNING:
                warning.predefinedSet |= SET_BIT(W3C_RISK_DETECTED_WARN);
                break;
            case TRANSACTION_CHECK_RISK_MALICIOUS:
                warning.predefinedSet |= SET_BIT(W3C_THREAT_DETECTED_WARN);
                break;
            default:
                break;
        }
        warning.reportProvider = G_transaction_check_info.partner;
        warning.providerMessage = get_tx_simulation_category_str();
        warning.reportUrl = G_transaction_check_info.tiny_url;
    }
}

/**
 * @brief Retrieve the TX Simulation risk string.
 *
 * @return risk as a string
 */
const char *get_tx_simulation_risk_str(void) {
    switch (G_transaction_check_info.risk) {
        case TRANSACTION_CHECK_RISK_BENIGN:
            return "BENIGN";
        case TRANSACTION_CHECK_RISK_WARNING:
            return "RISK (WARNING)";
        case TRANSACTION_CHECK_RISK_MALICIOUS:
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
    switch (G_transaction_check_info.risk) {
        case TRANSACTION_CHECK_RISK_WARNING:
            switch (G_transaction_check_info.category) {
                case TRANSACTION_CHECK_CATEGORY_ADDRESS:
                    return "This transaction involves a suspicious address. "
                           "It might not be safe to continue.";
                case TRANSACTION_CHECK_CATEGORY_DAPP:
                    return "This transaction involves a suspicious dApp. "
                           "It might not be safe to continue.";
                case TRANSACTION_CHECK_CATEGORY_LOSING_OPERATION:
                    return "This transaction could end in a loss. "
                           "Check transaction details carefully before signing.";
                default:
                    return "This transaction might be malicious. It might not be safe to continue. "
                           "Tap the QR code icon for more details.";
                    break;
            }
            break;
        case TRANSACTION_CHECK_RISK_MALICIOUS:
            switch (G_transaction_check_info.category) {
                case TRANSACTION_CHECK_CATEGORY_ADDRESS:
                    return "This transaction involves a malicious address. "
                           "Your assets will most likely be stolen.";
                case TRANSACTION_CHECK_CATEGORY_DAPP:
                    return "This dApp is linked to a scammer. "
                           "Your assets will most likely be stolen.";
                default:
                    return "This request is malicious. Your assets will most likely be stolen. "
                           "View full report for details.";
            }
        case TRANSACTION_CHECK_RISK_BENIGN:
            // No warning screen is displayed for BENIGN level thus no warning text is determined
            return "Benign";
        default:
            // Unreachable
            return NULL;
    }
}

#endif  // HAVE_TRANSACTION_CHECKS
