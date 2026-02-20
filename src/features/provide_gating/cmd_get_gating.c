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
#include "tlv.h"
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

enum {
    TAG_STRUCTURE_TYPE = 0x01,
    TAG_STRUCTURE_VERSION = 0x02,
    TAG_ADDRESS = 0x22,
    TAG_CHAIN_ID = 0x23,
    TAG_HASH_SELECTOR = 0x40,
    TAG_INTRO_MSG = 0x82,
    TAG_TINY_URL = 0x83,
    TAG_TX_TYPE = 0x84,
    TAG_DER_SIGNATURE = 0x15,
};

enum {
    BIT_STRUCTURE_TYPE,
    BIT_STRUCTURE_VERSION,
    BIT_ADDRESS,
    BIT_CHAIN_ID,
    BIT_HASH_SELECTOR,
    BIT_INTRO_MSG,
    BIT_TINY_URL,
    BIT_TX_TYPE,
    BIT_DER_SIGNATURE,
};

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
    uint8_t *sig;
    cx_sha256_t hash_ctx;
    uint32_t rcv_flags;
} s_gating_ctx;

// Global structure to store the tx gating parameters
static gating_t *GATING = NULL;
static nbgl_preludeDetails_t prelude_details = {0};
static nbgl_genericDetails_t generic_details = {0};

// Macros to check the field length
#define CHECK_FIELD_LENGTH(tag, len, expected)  \
    do {                                        \
        if (len != expected) {                  \
            PRINTF("%s Size mismatch!\n", tag); \
            return SWO_INCORRECT_DATA;          \
        }                                       \
    } while (0)
#define CHECK_FIELD_OVERFLOW(tag, field, len)   \
    do {                                        \
        if (len >= sizeof(field)) {             \
            PRINTF("%s Size overflow!\n", tag); \
            return SWO_INSUFFICIENT_MEMORY;     \
        }                                       \
    } while (0)

// Macro to check the field value
#define CHECK_FIELD_VALUE(tag, value, expected)  \
    do {                                         \
        if (value != expected) {                 \
            PRINTF("%s Value mismatch!\n", tag); \
            return SWO_INCORRECT_DATA;           \
        }                                        \
    } while (0)

// Macro to check the field value
#define CHECK_EMPTY_BUFFER(tag, field, len)   \
    do {                                      \
        if (memcmp(field, empty, len) == 0) { \
            PRINTF("%s Zero buffer!\n", tag); \
            return SWO_INCORRECT_DATA;        \
        }                                     \
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
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_struct_type(const s_tlv_data *data, s_gating_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_TYPE", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_TYPE", data->value[0], TYPE_GATED_SIGNING);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_TYPE);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the STRUCTURE_VERSION value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_struct_version(const s_tlv_data *data, s_gating_ctx *context) {
    CHECK_FIELD_LENGTH("STRUCTURE_VERSION", data->length, 1);
    CHECK_FIELD_VALUE("STRUCTURE_VERSION", data->value[0], STRUCT_VERSION);
    context->rcv_flags |= SET_BIT(BIT_STRUCTURE_VERSION);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the HASH / SELECTOR value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 *
 * @note This field can be either
 *  - function selector (4 bytes for SignTX)
 *  - schema hash (for eip712)
 */
static uint16_t parse_hash_selector(const s_tlv_data *data, s_gating_ctx *context) {
    uint8_t empty[HASH_SIZE] = {0};
    if ((data->length != SELECTOR_SIZE) && (data->length != sizeof(eip712_context->schema_hash))) {
        PRINTF("HASH_SELECTOR Size mismatch!\n");
        return SWO_INCORRECT_DATA;
    }
    CHECK_EMPTY_BUFFER("HASH_SELECTOR", data->value, data->length);
    COPY_FIELD(context->gating->hash_selector, data);
    context->rcv_flags |= SET_BIT(BIT_HASH_SELECTOR);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the ADDRESS value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_address(const s_tlv_data *data, s_gating_ctx *context) {
    uint8_t empty[ADDRESS_LENGTH] = {0};
    CHECK_FIELD_LENGTH("ADDRESS", data->length, ADDRESS_LENGTH);
    CHECK_EMPTY_BUFFER("ADDRESS", data->value, data->length);
    COPY_FIELD(context->gating->addr, data);
    context->rcv_flags |= SET_BIT(BIT_ADDRESS);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the CHAIN_ID value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_chain_id(const s_tlv_data *data, s_gating_ctx *context) {
    uint64_t chain_id;
    uint64_t max_range;

    CHECK_FIELD_LENGTH("CHAIN_ID", data->length, sizeof(uint64_t));
    // Check if the chain ID is supported
    // https://github.com/ethereum/EIPs/blob/master/EIPS/eip-2294.md
    max_range = 0x7FFFFFFFFFFFFFDB;
    chain_id = u64_from_BE(data->value, data->length);
    // Check if the chain_id is supported
    if ((chain_id > max_range) || (chain_id == 0)) {
        PRINTF("Unsupported chain ID: %llu\n", chain_id);
        return SWO_INCORRECT_DATA;
    }

    context->gating->chain_id = chain_id;
    context->rcv_flags |= SET_BIT(BIT_CHAIN_ID);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the INTRO_MSG value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_intro_msg(const s_tlv_data *data, s_gating_ctx *context) {
    CHECK_FIELD_OVERFLOW("INTRO_MSG", context->gating->intro_msg, data->length);
    // Check if the name is printable
    if (!is_printable((const char *) data->value, data->length)) {
        PRINTF("INTRO_MSG is not printable!\n");
        return SWO_INCORRECT_DATA;
    }
    COPY_FIELD(context->gating->intro_msg, data);
    context->rcv_flags |= SET_BIT(BIT_INTRO_MSG);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the TINY_URL value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_tiny_url(const s_tlv_data *data, s_gating_ctx *context) {
    CHECK_FIELD_OVERFLOW("TINY_URL", context->gating->tiny_url, data->length);
    // Check if the name is printable
    if (!is_printable((const char *) data->value, data->length)) {
        PRINTF("TINY_URL is not printable!\n");
        return SWO_INCORRECT_DATA;
    }
    COPY_FIELD(context->gating->tiny_url, data);
    context->rcv_flags |= SET_BIT(BIT_TINY_URL);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the TX_TYPE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_type(const s_tlv_data *data, s_gating_ctx *context) {
    CHECK_FIELD_LENGTH("TX_TYPE", data->length, sizeof(context->gating->type));
    if (data->value[0] >= TX_TYPE_TYPED_DATA) {
        PRINTF("TX_TYPE out of range: %d\n", data->value[0]);
        return SWO_INCORRECT_DATA;
    }
    context->gating->type = data->value[0] + 1;  // Because 0 is "unknown"
    context->rcv_flags |= SET_BIT(BIT_TX_TYPE);
    return SWO_SUCCESS;
}

/**
 * @brief Parse the SIGNATURE value.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static uint16_t parse_signature(const s_tlv_data *data, s_gating_ctx *context) {
    context->sig_size = data->length;
    context->sig = (uint8_t *) data->value;
    context->rcv_flags |= SET_BIT(BIT_DER_SIGNATURE);
    return SWO_SUCCESS;
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
    cx_err_t error = CX_INTERNAL_ERROR;
    bool ret_code = false;

    CX_CHECK(
        cx_hash_no_throw((cx_hash_t *) &context->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH));

    CX_CHECK(check_signature_with_pubkey("Gating Signing",
                                         hash,
                                         sizeof(hash),
                                         NULL,
                                         0,
                                         CERTIFICATE_PUBLIC_KEY_USAGE_GATED_SIGNING,
                                         (uint8_t *) (context->sig),
                                         context->sig_size));

    ret_code = true;
end:
    return ret_code;
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
    uint32_t expected_fields;

    expected_fields = (1 << BIT_STRUCTURE_TYPE) | (1 << BIT_STRUCTURE_VERSION) |
                      (1 << BIT_TX_TYPE) | (1 << BIT_ADDRESS) | (1 << BIT_INTRO_MSG) |
                      (1 << BIT_TINY_URL) | (1 << BIT_DER_SIGNATURE);

    switch (context->gating->type) {
        case TX_TYPE_TRANSACTION:
            // For SignTx, we expect the chain ID
            expected_fields |= (1 << BIT_CHAIN_ID);
            break;
        case TX_TYPE_TYPED_DATA:
            // For EIP-712, we expect the schema hash
            expected_fields |= (1 << BIT_HASH_SELECTOR);
            break;
        default:
            break;
    }

    return ((context->rcv_flags & expected_fields) == expected_fields);
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
 * @brief Parse the received TLV.
 *
 * @param[in] data the tlv data
 * @param[in] context Gating context
 * @return APDU Response code
 */
static bool handle_gating_tlv(const s_tlv_data *data, s_gating_ctx *context) {
    uint16_t sw = SWO_PARAMETER_ERROR_NO_INFO;

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
        case TAG_HASH_SELECTOR:
            sw = parse_hash_selector(data, context);
            break;
        case TAG_TX_TYPE:
            sw = parse_type(data, context);
            break;
        case TAG_INTRO_MSG:
            sw = parse_intro_msg(data, context);
            break;
        case TAG_TINY_URL:
            sw = parse_tiny_url(data, context);
            break;
        case TAG_DER_SIGNATURE:
            sw = parse_signature(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            sw = SWO_SUCCESS;
            break;
    }
    if ((sw == SWO_SUCCESS) && (data->tag != TAG_DER_SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    }
    return (sw == SWO_SUCCESS);
}

/**
 * @brief Parse the TLV payload containing the TX Gating parameters.
 *
 * @param[in] payload buffer received
 * @param[in] size of the buffer
 * @return whether the TLV payload was handled successfully or not
 */
static bool handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    bool parsing_ret;
    s_gating_ctx ctx = {0};

    if (mem_buffer_allocate((void **) &GATING, sizeof(gating_t)) == false) {
        PRINTF("Error: Not enough memory!\n");
        return false;
    }
    ctx.gating = GATING;

    // Reset the structures
    explicit_bzero(GATING, sizeof(gating_t));
    // Initialize the hash context
    cx_sha256_init(&ctx.hash_ctx);

    parsing_ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_gating_tlv, &ctx);
    if (!parsing_ret || !verify_fields(&ctx) || !verify_signature(&ctx)) {
        explicit_bzero(GATING, sizeof(gating_t));
        explicit_bzero(&ctx, sizeof(s_gating_ctx));
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
            PRINTF("Error: Unexpected P1 (%u)!\n", p1);
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
            // Get the transaction TO address
            contract = tmpContent.txContent.destination;
            PRINTF("[GATING] Tx TO address: %.*h\n", ADDRESS_LENGTH, contract);
            PRINTF("[GATING] Gating address: %.*h\n", ADDRESS_LENGTH, GATING->addr);

            // Get the implementation address for the received descriptor address
            address = (uint8_t *) get_implem_contract(&GATING->chain_id,
                                                      (const uint8_t *) contract,
                                                      (const uint8_t *) GATING->hash_selector);
            if (address != NULL) {
                PRINTF("[GATING] Checking implementation address: %.*h\n", ADDRESS_LENGTH, address);
                // Implementation address found, a proxy exists for this descriptor
                // We will check the implementation address against the Gating descriptor address
                if (memcmp(address, GATING->addr, ADDRESS_LENGTH) != 0) {
                    PRINTF("[GATING] Proxy Implem ADDRESS mismatch: %.*h != %.*h\n",
                           ADDRESS_LENGTH,
                           address,
                           ADDRESS_LENGTH,
                           GATING->addr);
                    return false;
                }
                // Then we will check the transaction TO address against the proxy contract address
                address = (uint8_t *) get_proxy_contract(&GATING->chain_id,
                                                         (const uint8_t *) GATING->addr,
                                                         (const uint8_t *) GATING->hash_selector);
                PRINTF("[GATING] Checking contract address: %.*h\n", ADDRESS_LENGTH, address);
            } else {
                PRINTF("[GATING] No proxy found for this descriptor address\n");
                // Implem addr is NULL, checking with Descriptor address
                address = (uint8_t *) GATING->addr;
            }
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
