#ifdef HAVE_DOMAIN_NAME

#include <os.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "common_utils.h"  // ARRAY_SIZE
#include "apdu_constants.h"
#include "domain_name.h"
#include "challenge.h"
#include "mem.h"
#include "hash_bytes.h"
#include "network.h"
#include "public_keys.h"
#include "tlv.h"

#define P1_FIRST_CHUNK     0x01
#define P1_FOLLOWING_CHUNK 0x00

#define ALGO_SECP256K1 1

#define SLIP_44_ETHEREUM 60

typedef enum {
    STRUCT_TYPE = 0x01,
    STRUCT_VERSION = 0x02,
    CHALLENGE = 0x12,
    SIGNER_KEY_ID = 0x13,
    SIGNER_ALGO = 0x14,
    SIGNATURE = 0x15,
    DOMAIN_NAME = 0x20,
    COIN_TYPE = 0x21,
    ADDRESS = 0x22
} e_tlv_tag;

typedef enum { KEY_ID_TEST = 0x00, KEY_ID_PROD = 0x03 } e_key_id;

typedef struct {
    bool valid;
    char *name;
    uint8_t addr[ADDRESS_LENGTH];
} s_domain_name_info;

typedef struct {
    e_key_id key_id;
    uint8_t input_sig_size;
    const uint8_t *input_sig;
    cx_sha256_t hash_ctx;
} s_sig_ctx;

typedef struct {
    s_domain_name_info *domain_name_info;
    s_sig_ctx sig_ctx;
} s_handler_param;

static uint8_t *g_payload = NULL;
static uint16_t g_payload_size = 0;
static uint16_t g_payload_expected_size = 0;
static s_domain_name_info g_domain_name_info;
char g_domain_name[DOMAIN_NAME_MAX_LENGTH + 1];

/**
 * Send a response APDU
 *
 * @param[in] success whether it should use \ref APDU_RESPONSE_OK
 * @param[in] off payload offset (0 if no data other than status word)
 */
static void response_to_domain_name(bool success, uint8_t off) {
    uint16_t sw;

    if (success) {
        sw = APDU_RESPONSE_OK;
    } else {
        sw = apdu_response_code;
    }
    U2BE_ENCODE(G_io_apdu_buffer, off, sw);

    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, off + 2);
}

/**
 * Checks if a domain name for the given chain ID and address is known
 *
 * Always wipes the content of \ref g_domain_name_info
 *
 * @param[in] chain_id given chain ID
 * @param[in] addr given address
 * @return whether there is or not
 */
bool has_domain_name(const uint64_t *chain_id, const uint8_t *addr) {
    bool ret = false;

    if (g_domain_name_info.valid) {
        // Check if chain ID is known to be Ethereum-compatible (same derivation path)
        if ((chain_is_ethereum_compatible(chain_id)) &&
            (memcmp(addr, g_domain_name_info.addr, ADDRESS_LENGTH) == 0)) {
            ret = true;
        }
    }
    memset(&g_domain_name_info, 0, sizeof(g_domain_name_info));
    return ret;
}

/**
 * Get uint from tlv data
 *
 * Get an unsigned integer from variable length tlv data (up to 4 bytes)
 *
 * @param[in] data tlv data
 * @param[out] value the returned value
 * @return whether it was successful
 */
static bool get_uint_from_data(const s_tlv_data *data, uint32_t *value) {
    uint8_t size_diff;
    uint8_t buffer[sizeof(uint32_t)];

    if (data->length > sizeof(buffer)) {
        PRINTF("Unexpectedly long value (%u bytes) for tag 0x%x\n", data->length, data->tag);
        return false;
    }
    size_diff = sizeof(buffer) - data->length;
    memset(buffer, 0, size_diff);
    memcpy(buffer + size_diff, data->value, data->length);
    *value = U4BE(buffer, 0);
    return true;
}

/**
 * Handler for tag \ref STRUCT_TYPE
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_struct_type(const s_tlv_data *data, s_handler_param *param) {
    (void) data;
    (void) param;
    return true;  // unhandled for now
}

/**
 * Handler for tag \ref STRUCT_VERSION
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_struct_version(const s_tlv_data *data, s_handler_param *param) {
    (void) data;
    (void) param;
    return true;  // unhandled for now
}

/**
 * Handler for tag \ref CHALLENGE
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_challenge(const s_tlv_data *data, s_handler_param *param) {
    uint32_t value;
    (void) param;

    if (!get_uint_from_data(data, &value)) {
        return false;
    }
    return (value == get_challenge());
}

/**
 * Handler for tag \ref SIGNER_KEY_ID
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_sign_key_id(const s_tlv_data *data, s_handler_param *param) {
    uint32_t value;

    if (!get_uint_from_data(data, &value) || (value > UINT8_MAX)) {
        return false;
    }
    param->sig_ctx.key_id = value;
    return true;
}

/**
 * Handler for tag \ref SIGNER_ALGO
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_sign_algo(const s_tlv_data *data, s_handler_param *param) {
    uint32_t value;

    (void) param;
    if (!get_uint_from_data(data, &value)) {
        return false;
    }
    return (value == ALGO_SECP256K1);
}

/**
 * Handler for tag \ref SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_signature(const s_tlv_data *data, s_handler_param *param) {
    param->sig_ctx.input_sig_size = data->length;
    param->sig_ctx.input_sig = data->value;
    return true;
}

/**
 * Tests if the given domain name character is valid (in our subset of allowed characters)
 *
 * @param[in] c given character
 * @return whether the character is valid
 */
static bool is_valid_domain_character(char c) {
    if (isalpha((int) c)) {
        if (!islower((int) c)) {
            return false;
        }
    } else if (!isdigit((int) c)) {
        switch (c) {
            case '.':
            case '-':
            case '_':
                break;
            default:
                return false;
        }
    }
    return true;
}

/**
 * Handler for tag \ref DOMAIN_NAME
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_domain_name(const s_tlv_data *data, s_handler_param *param) {
    if (data->length > DOMAIN_NAME_MAX_LENGTH) {
        PRINTF("Domain name too long! (%u)\n", data->length);
        return false;
    }
    // TODO: Remove once other domain name providers are supported
    if ((data->length < 5) || (strncmp(".eth", (char *) &data->value[data->length - 4], 4) != 0)) {
        PRINTF("Unexpected TLD!\n");
        return false;
    }
    for (int idx = 0; idx < data->length; ++idx) {
        if (!is_valid_domain_character(data->value[idx])) {
            PRINTF("Domain name contains non-allowed character! (0x%x)\n", data->value[idx]);
            return false;
        }
        param->domain_name_info->name[idx] = data->value[idx];
    }
    param->domain_name_info->name[data->length] = '\0';
    return true;
}

/**
 * Handler for tag \ref COIN_TYPE
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_coin_type(const s_tlv_data *data, s_handler_param *param) {
    uint32_t value;

    (void) param;
    if (!get_uint_from_data(data, &value)) {
        return false;
    }
    return (value == SLIP_44_ETHEREUM);
}

/**
 * Handler for tag \ref ADDRESS
 *
 * @param[in] data the tlv data
 * @param[in,out] param the parameter received from the parser
 * @return whether it was successful
 */
static bool handle_address(const s_tlv_data *data, s_handler_param *param) {
    if (data->length != ADDRESS_LENGTH) {
        return false;
    }
    memcpy(param->domain_name_info->addr, data->value, ADDRESS_LENGTH);
    return true;
}

/**
 * Verify the signature context
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] sig_ctx the signature context
 * @return whether it was successful
 */
static bool verify_signature(const s_sig_ctx *sig_ctx) {
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t verif_key;

    cx_hash((cx_hash_t *) &sig_ctx->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH);
    switch (sig_ctx->key_id) {
#ifdef HAVE_DOMAIN_NAME_TEST_KEY
        case KEY_ID_TEST:
#else
        case KEY_ID_PROD:
#endif
            cx_ecfp_init_public_key(CX_CURVE_256K1,
                                    DOMAIN_NAME_PUB_KEY,
                                    sizeof(DOMAIN_NAME_PUB_KEY),
                                    &verif_key);
            break;
        default:
            PRINTF("Error: Unknown metadata key ID %u\n", sig_ctx->key_id);
            return false;
    }
    if (!cx_ecdsa_verify(&verif_key,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         sizeof(hash),
                         sig_ctx->input_sig,
                         sig_ctx->input_sig_size)) {
        PRINTF("Domain name signature verification failed!\n");
#ifndef HAVE_BYPASS_SIGNATURES
        return false;
#endif
    }
    return true;
}

/**
 * Allocate and assign TLV payload
 *
 * @param[in] size size of the payload
 * @return whether it was successful
 */
static bool alloc_payload(uint16_t size) {
    if ((g_payload = mem_alloc(size)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    g_payload_expected_size = size;
    return true;
}

/**
 * Deallocate and unassign TLV payload
 */
static void free_payload(void) {
    mem_dealloc(g_payload_expected_size);
    g_payload = NULL;
    g_payload_size = 0;
    g_payload_expected_size = 0;
}

static bool handle_first_chunk(const uint8_t **data, uint8_t *length) {
    // check if no payload is already in memory
    if (g_payload != NULL) {
        free_payload();
        apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
        return false;
    }

    // check if we at least get the size
    if (*length < sizeof(g_payload_expected_size)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if (!alloc_payload(U2BE(*data, 0))) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }

    // skip the size so we can process it like a following chunk
    *data += sizeof(g_payload_expected_size);
    *length -= sizeof(g_payload_expected_size);
    return true;
}

static bool handle_all_received(void) {
    const s_tlv_handler handlers[] = {
        {.tag = STRUCT_TYPE, .func = (f_tlv_handler *) &handle_struct_type, .required = true},
        {.tag = STRUCT_VERSION, .func = (f_tlv_handler *) &handle_struct_version, .required = true},
        {.tag = CHALLENGE, .func = (f_tlv_handler *) &handle_challenge, .required = true},
        {.tag = SIGNER_KEY_ID, .func = (f_tlv_handler *) &handle_sign_key_id, .required = true},
        {.tag = SIGNER_ALGO, .func = (f_tlv_handler *) &handle_sign_algo, .required = true},
        {.tag = SIGNATURE, .func = (f_tlv_handler *) &handle_signature, .required = true},
        {.tag = DOMAIN_NAME, .func = (f_tlv_handler *) &handle_domain_name, .required = true},
        {.tag = COIN_TYPE, .func = (f_tlv_handler *) &handle_coin_type, .required = true},
        {.tag = ADDRESS, .func = (f_tlv_handler *) &handle_address, .required = true}};
    s_handler_param handler_param;
    s_tlv_sig sig = {.tag = SIGNATURE, .ctx = (cx_hash_t *) &handler_param.sig_ctx.hash_ctx};

    g_domain_name_info.name = g_domain_name;
    handler_param.domain_name_info = &g_domain_name_info;
    cx_sha256_init(&handler_param.sig_ctx.hash_ctx);
    if (!parse_tlv(g_payload,
                   g_payload_size,
                   handlers,
                   ARRAY_SIZE(handlers),
                   &handler_param,
                   &sig) ||
        !verify_signature(&handler_param.sig_ctx)) {
        free_payload();
        roll_challenge();  // prevent brute-force guesses
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    g_domain_name_info.valid = true;
    PRINTF("Registered : %s => %.*h\n",
           g_domain_name_info.name,
           ADDRESS_LENGTH,
           g_domain_name_info.addr);
    free_payload();
    roll_challenge();  // prevent replays
    return true;
}

/**
 * Handle domain name APDU
 *
 * @param[in] p1 first APDU instruction parameter
 * @param[in] p2 second APDU instruction parameter
 * @param[in] data APDU payload
 * @param[in] length payload size
 */
void handle_provide_domain_name(uint8_t p1, uint8_t p2, const uint8_t *data, uint8_t length) {
    (void) p2;
    if (p1 == P1_FIRST_CHUNK) {
        if (!handle_first_chunk(&data, &length)) {
            return response_to_domain_name(false, 0);
        }
    } else {
        // check if a payload is already in memory
        if (g_payload == NULL) {
            apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
            return response_to_domain_name(false, 0);
        }
    }

    if ((g_payload_size + length) > g_payload_expected_size) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        free_payload();
        PRINTF("TLV payload size mismatch!\n");
        return response_to_domain_name(false, 0);
    }
    // feed into tlv payload
    memcpy(g_payload + g_payload_size, data, length);
    g_payload_size += length;

    // everything has been received
    if (g_payload_size == g_payload_expected_size) {
        if (!handle_all_received()) {
            response_to_domain_name(false, 0);
        }
    }
    return response_to_domain_name(true, 0);
}

#endif  // HAVE_DOMAIN_NAME
