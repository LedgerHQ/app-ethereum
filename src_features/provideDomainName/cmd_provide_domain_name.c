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

#define P1_FIRST_CHUNK     0x01
#define P1_FOLLOWING_CHUNK 0x00

#define ALGO_SECP256K1 1

#define SLIP_44_ETHEREUM 60

#define DER_LONG_FORM_FLAG        0x80  // 8th bit set
#define DER_FIRST_BYTE_VALUE_MASK 0x7f

typedef enum { TLV_TAG, TLV_LENGTH, TLV_VALUE } e_tlv_step;

typedef enum {
    STRUCTURE_TYPE = 0x01,
    STRUCTURE_VERSION = 0x02,
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
    uint8_t *buf;
    uint16_t size;
    uint16_t expected_size;
} s_tlv_payload;

typedef struct {
    e_tlv_tag tag;
    uint8_t length;
    const uint8_t *value;
} s_tlv_data;

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

typedef bool(t_tlv_handler)(const s_tlv_data *data,
                            s_domain_name_info *domain_name_info,
                            s_sig_ctx *sig_ctx);

typedef struct {
    uint8_t tag;
    t_tlv_handler *func;
    uint8_t found;
} s_tlv_handler;

static s_tlv_payload g_tlv_payload = {0};
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
 * Handler for tag \ref STRUCTURE_TYPE
 *
 * @param[] data the tlv data
 * @param[] domain_name_info the domain name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_structure_type(const s_tlv_data *data,
                                  s_domain_name_info *domain_name_info,
                                  s_sig_ctx *sig_ctx) {
    (void) data;
    (void) domain_name_info;
    (void) sig_ctx;
    return true;  // unhandled for now
}

/**
 * Handler for tag \ref STRUCTURE_VERSION
 *
 * @param[] data the tlv data
 * @param[] domain_name_info the domain name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_structure_version(const s_tlv_data *data,
                                     s_domain_name_info *domain_name_info,
                                     s_sig_ctx *sig_ctx) {
    (void) data;
    (void) domain_name_info;
    (void) sig_ctx;
    return true;  // unhandled for now
}

/**
 * Handler for tag \ref CHALLENGE
 *
 * @param[in] data the tlv data
 * @param[] domain_name_info the domain name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_challenge(const s_tlv_data *data,
                             s_domain_name_info *domain_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;
    (void) domain_name_info;
    (void) sig_ctx;

    if (!get_uint_from_data(data, &value)) {
        return false;
    }
    return (value == get_challenge());
}

/**
 * Handler for tag \ref SIGNER_KEY_ID
 *
 * @param[in] data the tlv data
 * @param[] domain_name_info the domain name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_sign_key_id(const s_tlv_data *data,
                               s_domain_name_info *domain_name_info,
                               s_sig_ctx *sig_ctx) {
    uint32_t value;
    (void) domain_name_info;

    if (!get_uint_from_data(data, &value) || (value > UINT8_MAX)) {
        return false;
    }
    sig_ctx->key_id = value;
    return true;
}

/**
 * Handler for tag \ref SIGNER_ALGO
 *
 * @param[in] data the tlv data
 * @param[] domain_name_info the domain name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_sign_algo(const s_tlv_data *data,
                             s_domain_name_info *domain_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) domain_name_info;
    (void) sig_ctx;
    if (!get_uint_from_data(data, &value)) {
        return false;
    }
    return (value == ALGO_SECP256K1);
}

/**
 * Handler for tag \ref SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[] domain_name_info the domain name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_signature(const s_tlv_data *data,
                             s_domain_name_info *domain_name_info,
                             s_sig_ctx *sig_ctx) {
    (void) domain_name_info;
    sig_ctx->input_sig_size = data->length;
    sig_ctx->input_sig = data->value;
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
 * @param[out] domain_name_info the domain name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_domain_name(const s_tlv_data *data,
                               s_domain_name_info *domain_name_info,
                               s_sig_ctx *sig_ctx) {
    (void) sig_ctx;
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
        domain_name_info->name[idx] = data->value[idx];
    }
    domain_name_info->name[data->length] = '\0';
    return true;
}

/**
 * Handler for tag \ref COIN_TYPE
 *
 * @param[in] data the tlv data
 * @param[] domain_name_info the domain name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_coin_type(const s_tlv_data *data,
                             s_domain_name_info *domain_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) domain_name_info;
    (void) sig_ctx;
    if (!get_uint_from_data(data, &value)) {
        return false;
    }
    return (value == SLIP_44_ETHEREUM);
}

/**
 * Handler for tag \ref ADDRESS
 *
 * @param[in] data the tlv data
 * @param[out] domain_name_info the domain name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_address(const s_tlv_data *data,
                           s_domain_name_info *domain_name_info,
                           s_sig_ctx *sig_ctx) {
    (void) sig_ctx;
    if (data->length != ADDRESS_LENGTH) {
        return false;
    }
    memcpy(domain_name_info->addr, data->value, ADDRESS_LENGTH);
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
    cx_err_t error = CX_INTERNAL_ERROR;

    CX_CHECK(
        cx_hash_no_throw((cx_hash_t *) &sig_ctx->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH));
    switch (sig_ctx->key_id) {
#ifdef HAVE_DOMAIN_NAME_TEST_KEY
        case KEY_ID_TEST:
#else
        case KEY_ID_PROD:
#endif
            CX_CHECK(cx_ecfp_init_public_key_no_throw(CX_CURVE_256K1,
                                                      DOMAIN_NAME_PUB_KEY,
                                                      sizeof(DOMAIN_NAME_PUB_KEY),
                                                      &verif_key));
            break;
        default:
            PRINTF("Error: Unknown metadata key ID %u\n", sig_ctx->key_id);
            return false;
    }
    if (!cx_ecdsa_verify_no_throw(&verif_key,
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
end:
    return false;
}

/**
 * Calls the proper handler for the given TLV data
 *
 * Checks if there is a proper handler function for the given TLV tag and then calls it
 *
 * @param[in] handlers list of tag / handler function pairs
 * @param[in] handler_count number of handlers
 * @param[in] data the TLV data
 * @param[out] domain_name_info the domain name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_tlv_data(s_tlv_handler *handlers,
                            int handler_count,
                            const s_tlv_data *data,
                            s_domain_name_info *domain_name_info,
                            s_sig_ctx *sig_ctx) {
    t_tlv_handler *fptr;

    // check if a handler exists for this tag
    for (int idx = 0; idx < handler_count; ++idx) {
        if (handlers[idx].tag == data->tag) {
            handlers[idx].found += 1;
            fptr = PIC(handlers[idx].func);
            if (!(*fptr)(data, domain_name_info, sig_ctx)) {
                PRINTF("Error while handling tag 0x%x\n", handlers[idx].tag);
                return false;
            }
            break;
        }
    }
    return true;
}

/**
 * Checks if all the TLV tags were found during parsing
 *
 * @param[in,out] handlers list of tag / handler function pairs
 * @param[in] handler_count number of handlers
 * @return whether all tags were found
 */
static bool check_found_tlv_tags(s_tlv_handler *handlers, int handler_count) {
    // prevent missing or duplicated tags
    for (int idx = 0; idx < handler_count; ++idx) {
        if (handlers[idx].found != 1) {
            PRINTF("Found %u occurrence(s) of tag 0x%x in TLV!\n",
                   handlers[idx].found,
                   handlers[idx].tag);
            return false;
        }
    }
    return true;
}

/** Parse DER-encoded value
 *
 * Parses a DER-encoded value (up to 4 bytes long)
 * https://en.wikipedia.org/wiki/X.690
 *
 * @param[in] payload the TLV payload
 * @param[in,out] offset the payload offset
 * @param[out] value the parsed value
 * @return whether it was successful
 */
static bool parse_der_value(const s_tlv_payload *payload, size_t *offset, uint32_t *value) {
    bool ret = false;
    uint8_t byte_length;
    uint8_t buf[sizeof(*value)];

    if (value != NULL) {
        if (payload->buf[*offset] & DER_LONG_FORM_FLAG) {  // long form
            byte_length = payload->buf[*offset] & DER_FIRST_BYTE_VALUE_MASK;
            *offset += 1;
            if ((*offset + byte_length) > payload->size) {
                PRINTF("TLV payload too small for DER encoded value\n");
            } else {
                if (byte_length > sizeof(buf) || byte_length == 0) {
                    PRINTF("Unexpectedly long DER-encoded value (%u bytes)\n", byte_length);
                } else {
                    memset(buf, 0, (sizeof(buf) - byte_length));
                    memcpy(buf + (sizeof(buf) - byte_length), &payload->buf[*offset], byte_length);
                    *value = U4BE(buf, 0);
                    *offset += byte_length;
                    ret = true;
                }
            }
        } else {  // short form
            *value = payload->buf[*offset];
            *offset += 1;
            ret = true;
        }
    }
    return ret;
}

/**
 * Get DER-encoded value as an uint8
 *
 * Parses the value and checks if it fits in the given \ref uint8_t value
 *
 * @param[in] payload the TLV payload
 * @param[in,out] offset
 * @param[out] value the parsed value
 * @return whether it was successful
 */
static bool get_der_value_as_uint8(const s_tlv_payload *payload, size_t *offset, uint8_t *value) {
    bool ret = false;
    uint32_t tmp_value;

    if (value != NULL) {
        if (!parse_der_value(payload, offset, &tmp_value)) {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        } else {
            if (tmp_value <= UINT8_MAX) {
                *value = tmp_value;
                ret = true;
            } else {
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                PRINTF("TLV DER-encoded value larger than 8 bits\n");
            }
        }
    }
    return ret;
}

/**
 * Parse the TLV payload
 *
 * Does the TLV parsing but also the SHA-256 hash of the payload.
 *
 * @param[in] payload the raw TLV payload
 * @param[out] domain_name_info the domain name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool parse_tlv(const s_tlv_payload *payload,
                      s_domain_name_info *domain_name_info,
                      s_sig_ctx *sig_ctx) {
    s_tlv_handler handlers[] = {
        {.tag = STRUCTURE_TYPE, .func = &handle_structure_type, .found = 0},
        {.tag = STRUCTURE_VERSION, .func = &handle_structure_version, .found = 0},
        {.tag = CHALLENGE, .func = &handle_challenge, .found = 0},
        {.tag = SIGNER_KEY_ID, .func = &handle_sign_key_id, .found = 0},
        {.tag = SIGNER_ALGO, .func = &handle_sign_algo, .found = 0},
        {.tag = SIGNATURE, .func = &handle_signature, .found = 0},
        {.tag = DOMAIN_NAME, .func = &handle_domain_name, .found = 0},
        {.tag = COIN_TYPE, .func = &handle_coin_type, .found = 0},
        {.tag = ADDRESS, .func = &handle_address, .found = 0}};
    e_tlv_step step = TLV_TAG;
    s_tlv_data data;
    size_t offset = 0;
    size_t tag_start_off;

    cx_sha256_init(&sig_ctx->hash_ctx);
    // handle TLV payload
    while (offset < payload->size) {
        switch (step) {
            case TLV_TAG:
                tag_start_off = offset;
                if (!get_der_value_as_uint8(payload, &offset, &data.tag)) {
                    return false;
                }
                step = TLV_LENGTH;
                break;

            case TLV_LENGTH:
                if (!get_der_value_as_uint8(payload, &offset, &data.length)) {
                    return false;
                }
                step = TLV_VALUE;
                break;

            case TLV_VALUE:
                if (offset >= payload->size) {
                    return false;
                }
                data.value = &payload->buf[offset];
                if (!handle_tlv_data(handlers,
                                     ARRAY_SIZE(handlers),
                                     &data,
                                     domain_name_info,
                                     sig_ctx)) {
                    return false;
                }
                offset += data.length;
                if (data.tag != SIGNATURE) {  // the signature wasn't computed on itself
                    hash_nbytes(&payload->buf[tag_start_off],
                                (offset - tag_start_off),
                                (cx_hash_t *) &sig_ctx->hash_ctx);
                }
                step = TLV_TAG;
                break;

            default:
                return false;
        }
    }
    return check_found_tlv_tags(handlers, ARRAY_SIZE(handlers));
}

/**
 * Allocate and assign TLV payload
 *
 * @param[in] payload payload structure
 * @param[in] size size of the payload
 * @return whether it was successful
 */
static bool alloc_payload(s_tlv_payload *payload, uint16_t size) {
    if ((payload->buf = mem_alloc(size)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    payload->expected_size = size;
    return true;
}

/**
 * Deallocate and unassign TLV payload
 *
 * @param[in] payload payload structure
 */
static void free_payload(s_tlv_payload *payload) {
    mem_dealloc(payload->expected_size);
    memset(payload, 0, sizeof(*payload));
}

static bool handle_first_chunk(const uint8_t **data, uint8_t *length, s_tlv_payload *payload) {
    // check if no payload is already in memory
    if (payload->buf != NULL) {
        free_payload(payload);
        apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
        return false;
    }

    // check if we at least get the size
    if (*length < sizeof(payload->expected_size)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if (!alloc_payload(payload, U2BE(*data, 0))) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }

    // skip the size so we can process it like a following chunk
    *data += sizeof(payload->expected_size);
    *length -= sizeof(payload->expected_size);
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
    s_sig_ctx sig_ctx;

    (void) p2;
    if (p1 == P1_FIRST_CHUNK) {
        if (!handle_first_chunk(&data, &length, &g_tlv_payload)) {
            return response_to_domain_name(false, 0);
        }
    } else {
        // check if a payload is already in memory
        if (g_tlv_payload.buf == NULL) {
            apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
            return response_to_domain_name(false, 0);
        }
    }

    if ((g_tlv_payload.size + length) > g_tlv_payload.expected_size) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        free_payload(&g_tlv_payload);
        PRINTF("TLV payload size mismatch!\n");
        return response_to_domain_name(false, 0);
    }
    // feed into tlv payload
    memcpy(g_tlv_payload.buf + g_tlv_payload.size, data, length);
    g_tlv_payload.size += length;

    // everything has been received
    if (g_tlv_payload.size == g_tlv_payload.expected_size) {
        g_domain_name_info.name = g_domain_name;
        if (!parse_tlv(&g_tlv_payload, &g_domain_name_info, &sig_ctx) ||
            !verify_signature(&sig_ctx)) {
            free_payload(&g_tlv_payload);
            roll_challenge();  // prevent brute-force guesses
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return response_to_domain_name(false, 0);
        }
        g_domain_name_info.valid = true;
        PRINTF("Registered : %s => %.*h\n",
               g_domain_name_info.name,
               ADDRESS_LENGTH,
               g_domain_name_info.addr);
        free_payload(&g_tlv_payload);
        roll_challenge();  // prevent replays
    }
    return response_to_domain_name(true, 0);
}

#endif  // HAVE_DOMAIN_NAME
