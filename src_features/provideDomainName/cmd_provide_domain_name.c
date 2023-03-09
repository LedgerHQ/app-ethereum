#ifdef HAVE_DOMAIN_NAME

#include <os.h>
#include <stdint.h>
#include <string.h>
#include "utils.h"  // ARRAY_SIZE
#include "apdu_constants.h"
#include "domain_name.h"
#include "challenge.h"
#include "mem.h"
#include "hash_bytes.h"

static const uint8_t DOMAIN_NAME_PUB_KEY[] = {
#ifdef HAVE_DOMAIN_NAME_TEST_KEY
    0x04, 0xb9, 0x1f, 0xbe, 0xc1, 0x73, 0xe3, 0xba, 0x4a, 0x71, 0x4e, 0x01, 0x4e, 0xbc,
    0x82, 0x7b, 0x6f, 0x89, 0x9a, 0x9f, 0xa7, 0xf4, 0xac, 0x76, 0x9c, 0xde, 0x28, 0x43,
    0x17, 0xa0, 0x0f, 0x4f, 0x65, 0x0f, 0x09, 0xf0, 0x9a, 0xa4, 0xff, 0x5a, 0x31, 0x76,
    0x02, 0x55, 0xfe, 0x5d, 0xfc, 0x81, 0x13, 0x29, 0xb3, 0xb5, 0x0b, 0xe9, 0x91, 0x94,
    0xfc, 0xa1, 0x16, 0x19, 0xe6, 0x5f, 0x2e, 0xdf, 0xea
#else
    0x04, 0x6a, 0x94, 0xe7, 0xa4, 0x2c, 0xd0, 0xc3, 0x3f, 0xdf, 0x44, 0x0c, 0x8e, 0x2a,
    0xb2, 0x54, 0x2c, 0xef, 0xbe, 0x5d, 0xb7, 0xaa, 0x0b, 0x93, 0xa9, 0xfc, 0x81, 0x4b,
    0x9a, 0xcf, 0xa7, 0x5e, 0xb4, 0xe5, 0x3d, 0x6f, 0x00, 0x25, 0x94, 0xbd, 0xb6, 0x05,
    0xd9, 0xb5, 0xbd, 0xa9, 0xfa, 0x4b, 0x4b, 0xf3, 0xa5, 0x49, 0x6f, 0xd3, 0x16, 0x4b,
    0xae, 0xf5, 0xaf, 0xcf, 0x90, 0xe8, 0x40, 0x88, 0x71
#endif
};

#define P1_FIRST_CHUNK     0x01
#define P1_FOLLOWING_CHUNK 0x00

#define ALGO_SECP256K1 1

#define SLIP_44_ETHEREUM 60

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
 * @param[in] chain_id given chain ID
 * @param[in] addr given address
 * @return whether there is or not
 */
bool has_domain_name(const uint64_t *chain_id, const uint8_t *addr) {
    bool ret = false;

    if (g_domain_name_info.valid) {
        if ((*chain_id == ETHEREUM_MAINNET_CHAINID) &&
            (memcmp(addr, g_domain_name_info.addr, ADDRESS_LENGTH) == 0)) {
            ret = true;
        }
        memset(&g_domain_name_info, 0, sizeof(g_domain_name_info));
    }
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
    return get_uint_from_data(data, &value) && (value == get_challenge());
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
    return get_uint_from_data(data, &value) && (value == ALGO_SECP256K1);
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
        return false;
    }
    strncpy(domain_name_info->name, (const char *) data->value, data->length);
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
    return get_uint_from_data(data, &value) && (value == SLIP_44_ETHEREUM);
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
 * Feed the TLV data into the signature hash
 *
 * Continue the progressive SHA-256 hash of the signature context on the TLV data
 *
 * @param[in] sig_ctx the signature context
 */
static void feed_tlv_hash(const s_tlv_data *data, s_sig_ctx *sig_ctx) {
    hash_byte(data->tag, (cx_hash_t *) &sig_ctx->hash_ctx);
    hash_byte(data->length, (cx_hash_t *) &sig_ctx->hash_ctx);
    hash_nbytes(data->value, data->length, (cx_hash_t *) &sig_ctx->hash_ctx);
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
            PRINTF("Found %u occurence(s) of tag 0x%x in TLV!\n",
                   handlers[idx].found,
                   handlers[idx].tag);
            return false;
        }
    }
    return true;
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
    uint16_t offset = 0;

    cx_sha256_init(&sig_ctx->hash_ctx);
    // handle TLV payload
    while (offset < payload->size) {
        switch (step) {
            case TLV_TAG:
                data.tag = payload->buf[offset];
                offset += sizeof(data.tag);
                step = TLV_LENGTH;
                break;

            case TLV_LENGTH:
                data.length = payload->buf[offset];
                offset += sizeof(data.length);
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
                if (data.tag != SIGNATURE) {  // the signature wasn't computed on itself
                    feed_tlv_hash(&data, sig_ctx);
                }
                offset += data.length;
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
