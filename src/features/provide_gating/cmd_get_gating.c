/*******************************************************************************
 *   Ledger Ethereum App
 *   (c) 2025 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ********************************************************************************/

#ifdef HAVE_GATING_SUPPORT

#include "cmd_get_gating.h"
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
#include "common_utils.h"
#include "plugin_utils.h"
#include "mem_utils.h"
#include "utils.h"
#include "ui_logic.h"
#include "feature_signTx.h"
#include "proxy_info.h"
#include "context_712.h"
#include "schema_hash.h"

// Display the Dated Signing screen 1/X times
#define GATED_SIGNING_MAX_COUNT 10

#define TYPE_GATED_SIGNING 0x0D
#define STRUCT_VERSION     0x01

#define GATING_MSG_SIZE 100
#define GATING_URL_SIZE 30
#define HASH_SIZE       32

typedef enum {
    TX_TYPE_UNKNOWN,
    TX_TYPE_TRANSACTION,
    TX_TYPE_TYPED_DATA,
} tx_type_t;

typedef struct gating_s {
    uint64_t chain_id;
    const char hash_selector[HASH_SIZE];  // function selector for SignTx or schemaHash for EIP712
    const char intro_msg[GATING_MSG_SIZE + 1];  // +1 for the null terminator
    const char tiny_url[GATING_URL_SIZE + 1];   // +1 for the null terminator
    const char addr[ADDRESS_LENGTH];
    tx_type_t type;
} gating_t;

typedef struct {
    gating_t *gating;
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
    TLV_reception_t received_tags;
} s_gating_ctx;

// Global structure to store the tx gating parameters
static gating_t *GATING = NULL;
static nbgl_preludeDetails_t prelude_details = {0};
static nbgl_genericDetails_t generic_details = {0};

/**
 * @brief Parse the STRUCTURE_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_struct_type(const tlv_data_t *data, s_gating_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_type(data, TYPE_GATED_SIGNING);
}

/**
 * @brief Parse the STRUCTURE_VERSION value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_struct_version(const tlv_data_t *data, s_gating_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_version(data, STRUCT_VERSION);
}

/**
 * @brief Parse the HASH / SELECTOR value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 *
 * @note This field can be either
 *  - function selector (4 bytes for SignTX)
 *  - schema hash (32 bytes for eip712)
 */
static bool parse_hash_selector(const tlv_data_t *data, s_gating_ctx *context) {
    return tlv_get_selector(data,
                            (uint8_t *) context->gating->hash_selector,
                            sizeof(context->gating->hash_selector));
}

/**
 * @brief Parse the ADDRESS value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_address(const tlv_data_t *data, s_gating_ctx *context) {
    return tlv_get_address(data, (uint8_t *) context->gating->addr, true);
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_chain_id(const tlv_data_t *data, s_gating_ctx *context) {
    return tlv_get_chain_id(data, &context->gating->chain_id);
}

/**
 * @brief Parse the INTRO_MSG value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_intro_msg(const tlv_data_t *data, s_gating_ctx *context) {
    if (!tlv_get_printable_string(data,
                                  (char *) context->gating->intro_msg,
                                  0,
                                  sizeof(context->gating->intro_msg))) {
        PRINTF("INTRO_MSG: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the TINY_URL value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_tiny_url(const tlv_data_t *data, s_gating_ctx *context) {
    if (!tlv_get_printable_string(data,
                                  (char *) context->gating->tiny_url,
                                  0,
                                  sizeof(context->gating->tiny_url))) {
        PRINTF("TINY_URL: error\n");
        return false;
    }
    return true;
}

/**
 * @brief Parse the TX_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_type(const tlv_data_t *data, s_gating_ctx *context) {
    uint8_t value = 0;
    if (!tlv_get_uint8(data, &value, 0, TX_TYPE_TYPED_DATA - 1)) {
        PRINTF("TX_TYPE: error\n");
        return false;
    }
    context->gating->type = value + 1;  // Because 0 is "unknown"
    return true;
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool parse_signature(const tlv_data_t *data, s_gating_ctx *context) {
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

// Define TLV tags for Gating
#define GATING_TAGS(X)                                                       \
    X(0x01, TAG_STRUCTURE_TYPE, parse_struct_type, ENFORCE_UNIQUE_TAG)       \
    X(0x02, TAG_STRUCTURE_VERSION, parse_struct_version, ENFORCE_UNIQUE_TAG) \
    X(0x22, TAG_ADDRESS, parse_address, ENFORCE_UNIQUE_TAG)                  \
    X(0x23, TAG_CHAIN_ID, parse_chain_id, ENFORCE_UNIQUE_TAG)                \
    X(0x40, TAG_HASH_SELECTOR, parse_hash_selector, ENFORCE_UNIQUE_TAG)      \
    X(0x82, TAG_INTRO_MSG, parse_intro_msg, ENFORCE_UNIQUE_TAG)              \
    X(0x83, TAG_TINY_URL, parse_tiny_url, ENFORCE_UNIQUE_TAG)                \
    X(0x84, TAG_TX_TYPE, parse_type, ENFORCE_UNIQUE_TAG)                     \
    X(0x15, TAG_DER_SIGNATURE, parse_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration
static bool gating_common_handler(const tlv_data_t *data, s_gating_ctx *context);

// Generate TLV parser for Gating
DEFINE_TLV_PARSER(GATING_TAGS, &gating_common_handler, gating_tlv_parser)

/**
 * @brief Common handler called for all tags to hash them (except signature).
 *
 * @param[in] data the TLV data
 * @param[out] context Gating context
 * @return whether it was successful
 */
static bool gating_common_handler(const tlv_data_t *data, s_gating_ctx *context) {
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
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool verify_signature(s_gating_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (finalize_hash((cx_hash_t *) &context->hash_ctx, hash, sizeof(hash)) != true) {
        PRINTF("Could not finalize struct hash!\n");
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_GATED_SIGNING,
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
 * @param[in] context Gating context
 * @return whether it was successful
 */
static bool verify_fields(s_gating_ctx *context) {
    // Common required tags for all types
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                 TAG_STRUCTURE_TYPE,
                                 TAG_STRUCTURE_VERSION,
                                 TAG_TX_TYPE,
                                 TAG_ADDRESS,
                                 TAG_INTRO_MSG,
                                 TAG_TINY_URL,
                                 TAG_DER_SIGNATURE)) {
        return false;
    }

    switch (context->gating->type) {
        case TX_TYPE_TRANSACTION:
            // For SignTx, we expect the chain ID
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_CHAIN_ID)) {
                return false;
            }
            break;
        case TX_TYPE_TYPED_DATA:
            // For EIP-712, we expect the schema hash
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_HASH_SELECTOR)) {
                return false;
            }
            break;
        default:
            break;
    }

    return true;
}

/**
 * @brief Print the gating parameters.
 *
 * @param[in] context Gating context
 * Only for debug purpose.
 */
static void print_gating_info(s_gating_ctx *context) {
    uint8_t len = 0;
    PRINTF("****************************************************************************\n");
    PRINTF("[GATING] - Retrieved Gating descriptor:\n");
    PRINTF("[GATING] -    Address: %.*h\n", ADDRESS_LENGTH, context->gating->addr);
    if (context->gating->chain_id != 0) {
        PRINTF("[GATING] -    ChainID: %llu\n", context->gating->chain_id);
    }
    len = (context->gating->type == TX_TYPE_TRANSACTION) ? SELECTOR_SIZE : HASH_SIZE;
    if (allzeroes((const void *) context->gating->hash_selector, len) == 0) {
        PRINTF("[GATING] -    Hash Selector: %.*h\n", len, context->gating->hash_selector);
    }
    PRINTF("[GATING] -    Intro Msg: %s\n", context->gating->intro_msg);
    PRINTF("[GATING] -    Tiny URL: %s\n", context->gating->tiny_url);
}

/**
 * @brief Parse the TLV payload containing the TX Gating parameters.
 *
 * @param[in] buf buffer received
 * @return whether the TLV payload was handled successfully or not
 */
static bool handle_tlv_payload(const buffer_t *buf) {
    s_gating_ctx ctx = {0};

    if (mem_buffer_allocate((void **) &GATING, sizeof(gating_t)) == false) {
        PRINTF("Error: Not enough memory!\n");
        return false;
    }
    ctx.gating = GATING;

    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    if (!gating_tlv_parser(buf, &ctx, &ctx.received_tags)) {
        explicit_bzero(GATING, sizeof(gating_t));
        return false;
    }

    if (!verify_fields(&ctx) || !verify_signature(&ctx)) {
        explicit_bzero(GATING, sizeof(gating_t));
        return false;
    }

    print_gating_info(&ctx);
    return true;
}

/**
 * @brief Handle Gating APDU.
 *
 * @param[in] p1 APDU parameter 1 (indicates Data payload or Opt-In request)
 * @param[in] p2 APDU parameter 2 (indicates if the payload is the first chunk)
 * @param[in] data buffer received
 * @param[in] length of the buffer
 * @return APDU Response code
 */
uint16_t handle_gating(uint8_t p1, uint8_t p2, const uint8_t *data, uint8_t length) {
    uint16_t sw = SWO_PARAMETER_ERROR_NO_INFO;

    switch (p2) {
        case 0x00:
            if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, length, data, &handle_tlv_payload)) {
                sw = SWO_INCORRECT_DATA;
            } else {
                sw = SWO_SUCCESS;
            }
            break;
        default:
            PRINTF("Error: Unexpected P2 (%u)!\n", p2);
            sw = SWO_WRONG_P1_P2;
            break;
    }
    return sw;
}

/**
 * @brief Clear the Gating parameters.
 *
 */
void clear_gating(void) {
    mem_buffer_cleanup((void **) &GATING);
}

/**
 * @brief Check the TYPE in Gating payload.
 *
 * @return whether it was successful
 */
static bool check_gating_type(void) {
    switch (GATING->type) {
        case TX_TYPE_TRANSACTION:
            if (appState != APP_STATE_SIGNING_TX) {
                PRINTF("[GATING] Type mismatch: %u != %u\n", GATING->type, TX_TYPE_TRANSACTION);
                return false;
            }
            break;
        case TX_TYPE_TYPED_DATA:
            if (appState != APP_STATE_SIGNING_EIP712) {
                PRINTF("[GATING] Type mismatch: %u != %u\n", GATING->type, TX_TYPE_TYPED_DATA);
                return false;
            }
            break;
        default:
            PRINTF("[GATING] Invalid Type: %u\n", GATING->type);
            return false;
    }
    return true;
}

/**
 * @brief Check the TO_ADDRESS vs Gating payload.
 *
 * @return whether it was successful
 */
static bool check_gating_address(void) {
    uint8_t *address = NULL;
    uint8_t *contract = NULL;

    if (allzeroes((const void *) GATING->addr, ADDRESS_LENGTH)) {
        PRINTF("[GATING] TO addr missing\n");
        return false;
    }
    switch (GATING->type) {
        case TX_TYPE_TRANSACTION:
            // Get the implementation address for the received descriptor address
            address = (uint8_t *) get_implem_contract(&GATING->chain_id,
                                                      (const uint8_t *) GATING->addr,
                                                      (const uint8_t *) GATING->hash_selector);
            if (address == NULL) {
                // Implem addr is NULL, checking with Descriptor address
                address = (uint8_t *) GATING->addr;
            }
            contract = tmpContent.txContent.destination;
            break;
        case TX_TYPE_TYPED_DATA:
            address = (uint8_t *) GATING->addr;
            contract = eip712_context->contract_addr;
            break;
        default:
            return true;
    }

    if (memcmp(address, contract, ADDRESS_LENGTH) != 0) {
        PRINTF("[GATING] ADDRESS mismatch: %.*h != %.*h\n",
               ADDRESS_LENGTH,
               address,
               ADDRESS_LENGTH,
               contract);
        return false;
    }
    return true;
}

/**
 * @brief Check the CHAIN_ID vs Gating payload.
 *
 * @return whether it was successful
 */
static bool check_gating_chain_id(void) {
    uint64_t chain_id = 0;
    switch (GATING->type) {
        case TX_TYPE_TRANSACTION:
            chain_id = get_tx_chain_id();
            if (GATING->chain_id != chain_id) {
                PRINTF("[GATING] Chain_ID mismatch: %llu != %llu\n", GATING->chain_id, chain_id);
                return false;
            }
            break;
        case TX_TYPE_TYPED_DATA:
            chain_id = eip712_context->chain_id;
            // For EIP-712, the chain_id is optional, and be 0 in the descriptor (any chain)
            if ((GATING->chain_id != 0) && (GATING->chain_id != chain_id)) {
                PRINTF("[GATING] Chain_ID mismatch: %llu != %llu\n", GATING->chain_id, chain_id);
                return false;
            }
            break;
        default:
            break;
    }
    return true;
}

/**
 * @brief Check the SELECTOR vs Gating payload.
 *
 * @return whether it was successful
 */
static bool check_gating_selector(void) {
    switch (GATING->type) {
        case TX_TYPE_TRANSACTION:
            // Check if the descriptor is set
            if (allzeroes((const void *) GATING->hash_selector, SELECTOR_SIZE)) {
                break;
            }
            if (memcmp(GATING->hash_selector, txContext.selector_bytes, SELECTOR_SIZE) != 0) {
                PRINTF("[GATING] SELECTOR mismatch: %.*h != %.*h\n",
                       SELECTOR_SIZE,
                       GATING->hash_selector,
                       SELECTOR_SIZE,
                       txContext.selector_bytes);
                return false;
            }
            break;
        case TX_TYPE_TYPED_DATA:
            if (compute_schema_hash() == false) {
                PRINTF("[GATING] Failed to compute schema hash\n");
                return false;
            }
            if (memcmp(GATING->hash_selector,
                       eip712_context->schema_hash,
                       sizeof(eip712_context->schema_hash)) != 0) {
                PRINTF("[GATING] schemaHash mismatch: %.*h != %.*h\n",
                       sizeof(eip712_context->schema_hash),
                       GATING->hash_selector,
                       sizeof(eip712_context->schema_hash),
                       eip712_context->schema_hash);
                return false;
            }
            break;
        default:
            break;
    }
    return true;
}

/**
 * @brief Check the TX vs Gating parameters (CHAIN_ID, ADDRESS, SELECTOR).
 *
 * @return whether it was successful
 */
static bool check_tx_gating_params(void) {
    if (check_gating_type() == false) {
        return false;
    }
    if (check_gating_chain_id() == false) {
        return false;
    }
    if (check_gating_address() == false) {
        return false;
    }
    if (check_gating_selector() == false) {
        return false;
    }
    return true;
}

/**
 * @brief Configure the warning prelude set for the NBGL review flows.
 *
 */
static void set_gating_ui_screen(void) {
    explicit_bzero(&prelude_details, sizeof(prelude_details));
    explicit_bzero(&generic_details, sizeof(generic_details));

    generic_details.title = "Discover safer signing";
#ifdef SCREEN_SIZE_WALLET
    generic_details.type = QRCODE_WARNING;
    generic_details.qrCode.url = GATING->tiny_url;
    generic_details.qrCode.text1 = GATING->tiny_url;
    generic_details.qrCode.text2 = "Discover a safer way to sign your transactions";
    generic_details.qrCode.centered = true;
#endif

    prelude_details.icon = &ICON_LEDGER;
#ifdef SCREEN_SIZE_WALLET
    prelude_details.title = "There is a safer\nway to sign";
    prelude_details.description = GATING->intro_msg;
#else
    prelude_details.title = "\bA safer way exists:";
    prelude_details.description = GATING->tiny_url;
#endif
    prelude_details.buttonText = "Learn more";
    prelude_details.footerText = "Continue to blind signing";
    prelude_details.details = &generic_details;

    warning.prelude = &prelude_details;
}

/**
 * @brief Configure the warning prelude set for the NBGL review flows.
 *
 * @return whether the descriptor corresponds to the current transaction
 */
bool set_gating_warning(void) {
    uint8_t counter = 0;

    if (GATING == NULL) {
        PRINTF("[GATING] Descriptor not received\n");
        return true;
    }

    // Gated signing received => Verify parameters of the Transaction
    if (check_tx_gating_params() == false) {
        PRINTF("[GATING] Parameters mismatch\n");
        return false;
    }

    // Check the counter
    counter = N_storage.gating_counter + 1;
    PRINTF("[GATING] Counter: %d/%d\n", counter, GATED_SIGNING_MAX_COUNT);
    nvm_write((void *) &N_storage.gating_counter, (void *) &counter, sizeof(counter));
    if (((counter - 1) % GATED_SIGNING_MAX_COUNT) != 0) {
        PRINTF("[GATING] Skip gating screen\n");
        return true;
    }

    // Gated signing valid => Adapt the UI screens
    set_gating_ui_screen();
    return true;
}

#endif  // HAVE_GATING_SUPPORT
