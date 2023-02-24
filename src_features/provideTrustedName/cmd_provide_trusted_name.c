#ifdef HAVE_TRUSTED_NAME

#include <os.h>
#include <stdint.h>
#include <string.h>
#include "utils.h" // ARRAY_SIZE
#include "apdu_constants.h"
#include "trusted_name.h"
#include "challenge.h"
#include "mem.h"
#include "mem_utils.h"
#include "hash_bytes.h"

static const uint8_t TRUSTED_NAME_PUB_KEY[] = {
#ifdef HAVE_TRUSTED_NAME_TEST_KEY
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

char trusted_name[TRUSTED_NAME_MAX_LENGTH + 1];

typedef struct {
    bool valid;
    char *name;
    uint8_t addr[ADDRESS_LENGTH];
} s_trusted_name_info;

static s_trusted_name_info g_trusted_name_info;

#define P1_FIRST_CHUNK      0x01
#define P1_FOLLOWING_CHUNK  0x00

#define PROD_KEY_ID         0x03
#define TEST_KEY_ID         0x00

#define SLIP_44_ETHEREUM    60

typedef enum {
    STRUCTURE_TYPE = 0x01,
    STRUCTURE_VERSION = 0x02,
    CHALLENGE = 0x12,
    SIGNER_KEY_ID = 0x13,
    SIGNER_ALGO = 0x14,
    SIGNATURE = 0x15,
    TRUSTED_NAME = 0x20,
    COIN_TYPE = 0x21,
    ADDRESS = 0x22
} e_tlv_tag;

typedef enum {
    TLV_TAG,
    TLV_LENGTH,
    TLV_VALUE
} e_tlv_step;

typedef struct {
    uint8_t *ptr;
    uint16_t size;
    uint16_t expected_size;
} s_tlv_payload;

typedef struct {
    e_tlv_tag tag;
    uint8_t length;
    const uint8_t *value;
} s_tlv_data;

typedef struct {
    uint8_t key_id;
    uint8_t input_sig_size;
    const uint8_t *input_sig;
    cx_sha256_t hash_ctx;
} s_sig_ctx;

typedef bool (*t_tlv_handler)(const s_tlv_data *data,
                              s_trusted_name_info *trusted_name_info,
                              s_sig_ctx *sig_ctx);

typedef struct {
    uint8_t tag;
    t_tlv_handler func;
} s_tlv_handler;

static s_tlv_payload g_tlv_payload = { 0 };


/**
 * Send a response APDU
 *
 * @param[in] success whether it should use \ref APDU_RESPONSE_OK
 * @param[in] off payload offset (0 if no data other than status word)
 */
static void response_to_trusted_name(bool success, uint8_t off) {
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
 * Verify the trusted name in memory with the given address and chain ID
 *
 * @param[in] chain_id given chain ID
 * @param[in] addr given address
 * @return whether it matches or not
 */
bool verify_trusted_name(const uint64_t *chain_id, const uint8_t *addr) {
    (void)chain_id;
    if (g_trusted_name_info.valid) {
        if ((*chain_id == ETHEREUM_MAINNET_CHAINID)
            && (memcmp(addr, g_trusted_name_info.addr, ADDRESS_LENGTH) == 0)) {
            return true;
        }
        memset(&g_trusted_name_info, 0, sizeof(g_trusted_name_info));
    }
    return false;
}

static uint32_t get_uint_from_tlv(const s_tlv_data *data, uint32_t *value) {
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

static bool handle_structure_type(const s_tlv_data *data,
                                  s_trusted_name_info *trusted_name_info,
                                  s_sig_ctx *sig_ctx) {
    (void)data;
    (void)trusted_name_info;
    (void)sig_ctx;
    return true; // unhandled for now
}

static bool handle_structure_version(const s_tlv_data *data,
                                     s_trusted_name_info *trusted_name_info,
                                     s_sig_ctx *sig_ctx) {
    (void)data;
    (void)trusted_name_info;
    (void)sig_ctx;
    return true; // unhandled for now
}

static bool handle_challenge(const s_tlv_data *data,
                             s_trusted_name_info *trusted_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void)trusted_name_info;
    (void)sig_ctx;
    return get_uint_from_tlv(data, &value) && (value == get_challenge());
}

static bool handle_signer_key_id(const s_tlv_data *data,
                                 s_trusted_name_info *trusted_name_info,
                                 s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void)trusted_name_info;
    if (!get_uint_from_tlv(data, &value) || (value > UINT8_MAX)) {
        return false;
    }
    sig_ctx->key_id = value;
    return true;
}

static bool handle_algo(const s_tlv_data *data,
                        s_trusted_name_info *trusted_name_info,
                        s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void)trusted_name_info;
    (void)sig_ctx;
    return get_uint_from_tlv(data, &value) && (value == 1);
}

static bool handle_signature(const s_tlv_data *data,
                             s_trusted_name_info *trusted_name_info,
                             s_sig_ctx *sig_ctx) {
    (void)trusted_name_info;
    sig_ctx->input_sig_size = data->length;
    sig_ctx->input_sig = data->value;
    return true;
}

static bool handle_trusted_name(const s_tlv_data *data,
                                s_trusted_name_info *trusted_name_info,
                                s_sig_ctx *sig_ctx) {
    (void)sig_ctx;
    strncpy(trusted_name_info->name,
            (const char *)data->value,
            data->length);
    return true;
}

static bool handle_coin_type(const s_tlv_data *data,
                             s_trusted_name_info *trusted_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void)trusted_name_info;
    (void)sig_ctx;
    return get_uint_from_tlv(data, &value) && (value == SLIP_44_ETHEREUM);
}

static bool handle_address(const s_tlv_data *data,
                           s_trusted_name_info *trusted_name_info,
                           s_sig_ctx *sig_ctx) {

    (void)sig_ctx;
    if (data->length != ADDRESS_LENGTH) {
        return false;
    }
    memcpy(trusted_name_info->addr, data->value, ADDRESS_LENGTH);
    return true;
}


static bool verify_signature(s_sig_ctx *sig_ctx) {
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t verif_key;

    cx_hash((cx_hash_t *) &sig_ctx->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH);
    switch (sig_ctx->key_id) {
#ifdef HAVE_TRUSTED_NAME_TEST_KEY
        case TEST_KEY_ID:
#else
        case PROD_KEY_ID:
#endif
            cx_ecfp_init_public_key(CX_CURVE_256K1,
                                    TRUSTED_NAME_PUB_KEY,
                                    sizeof(TRUSTED_NAME_PUB_KEY),
                                    &verif_key);
            break;
        default:
            PRINTF("Error: Unknown metadata key ID %u\n", sig_ctx->key_id);
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
    }
    if (!cx_ecdsa_verify(&verif_key,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         sizeof(hash),
                         sig_ctx->input_sig,
                         sig_ctx->input_sig_size)) {
        PRINTF("Trusted name signature verification failed!\n");
#ifndef HAVE_BYPASS_SIGNATURES
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
#endif
    }
    return true;
}

static void feed_tlv_hash(const s_tlv_data *data, s_sig_ctx *sig_ctx) {
    hash_byte(data->tag, (cx_hash_t *) &sig_ctx->hash_ctx);
    hash_byte(data->length, (cx_hash_t *) &sig_ctx->hash_ctx);
    hash_nbytes(data->value, data->length, (cx_hash_t *) &sig_ctx->hash_ctx);
}

static bool parse_tlv(s_tlv_payload *payload,
                      s_trusted_name_info *trusted_name_info,
                      s_sig_ctx *sig_ctx) {
    s_tlv_handler handlers[] = {
        { .tag = STRUCTURE_TYPE, .func = &handle_structure_type },
        { .tag = STRUCTURE_VERSION, .func = &handle_structure_version },
        { .tag = CHALLENGE, .func = &handle_challenge },
        { .tag = SIGNER_KEY_ID, .func = &handle_signer_key_id },
        { .tag = SIGNER_ALGO, .func = &handle_algo },
        { .tag = SIGNATURE, .func = &handle_signature },
        { .tag = TRUSTED_NAME, .func = &handle_trusted_name },
        { .tag = COIN_TYPE, .func = &handle_coin_type },
        { .tag = ADDRESS, .func = &handle_address }
    };
    e_tlv_step step = TLV_TAG;
    s_tlv_data data;
    uint16_t offset = 0;

    cx_sha256_init(&sig_ctx->hash_ctx);
    // handle TLV payload
    while (offset < payload->size) {
        switch (step) {
            case TLV_TAG:
                data.tag = payload->ptr[offset];
                offset += sizeof(data.tag);
                step = TLV_LENGTH;
                break;

            case TLV_LENGTH:
                data.length = payload->ptr[offset];
                offset += sizeof(data.length);
                step = TLV_VALUE;
                break;

            case TLV_VALUE:
                if (offset >= payload->size) {
                    apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                    return false;
                }
                data.value = &payload->ptr[offset];
                // check if a handler exists for this tag
                for (int idx = 0; idx < (int)ARRAY_SIZE(handlers); ++idx) {
                    if (handlers[idx].tag == data.tag) {
                        t_tlv_handler fptr = PIC(handlers[idx].func);
                        if (!(*fptr)(&data, trusted_name_info, sig_ctx)) {
                            return false;
                        }
                        break;
                    }
                }
                if (data.tag != SIGNATURE) { // the signature wasn't computed on itself
                    feed_tlv_hash(&data, sig_ctx);
                }
                offset += data.length;
                step = TLV_TAG;
                break;

            default:
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                return false;
        }
    }
    return true;
}

static bool set_payload(s_tlv_payload *payload, uint16_t size) {
    if ((payload->ptr = mem_alloc(size)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }
    payload->expected_size = size;
    return true;
}

static void unset_payload(s_tlv_payload *payload) {
    mem_dealloc(payload->expected_size);
    payload->ptr = NULL;
}

static bool handle_first_chunk(const uint8_t **data, uint8_t *length, s_tlv_payload *payload) {
    // check if no payload is already in memory
    if (payload->ptr != NULL) {
        unset_payload(payload);
        apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
        return false;
    }

    // check if we at least get the size
    if (*length < sizeof(payload->expected_size)) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if (!set_payload(payload, U2BE(*data, 0))) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return false;
    }

    // skip the size so we can process it like a following chunk
    *data += sizeof(payload->expected_size);
    *length -= sizeof(payload->expected_size);
    return true;
}

/**
 * Handle trusted name APDU
 *
 * @param[in] p1 first APDU instruction parameter
 * @param[in] p2 second APDU instruction parameter
 * @param[in] data APDU payload
 * @param[in] length payload size
 */
void handle_provide_trusted_name(uint8_t p1, uint8_t p2, const uint8_t *data, uint8_t length) {
    s_sig_ctx sig_ctx;

    (void)p2;
    if (p1 == P1_FIRST_CHUNK) {
        if (!handle_first_chunk(&data, &length, &g_tlv_payload)) {
            return response_to_trusted_name(false, 0);
        }
    } else {
        // check if a payload is already in memory
        if (g_tlv_payload.ptr == NULL) {
            apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
            return response_to_trusted_name(false, 0);
        }
    }

    if (g_tlv_payload.size < g_tlv_payload.expected_size) {
        if ((g_tlv_payload.size + length) > g_tlv_payload.expected_size) {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            unset_payload(&g_tlv_payload);
            PRINTF("TLV payload size mismatch!\n");
            return response_to_trusted_name(false, 0);
        }
        // feed into tlv payload
        memcpy(g_tlv_payload.ptr + g_tlv_payload.size, data, length);
        g_tlv_payload.size += length;
    }

    // everything has been received
    if (g_tlv_payload.size == g_tlv_payload.expected_size) {
        g_trusted_name_info.name = trusted_name;
        if (!parse_tlv(&g_tlv_payload, &g_trusted_name_info, &sig_ctx) ||
            !verify_signature(&sig_ctx)) {
            unset_payload(&g_tlv_payload);
            roll_challenge(); // prevent brute-force guesses
            return response_to_trusted_name(false, 0);
        }
        g_trusted_name_info.valid = true;
        PRINTF("Registered : %s => %.*h\n", g_trusted_name_info.name,
                                            ADDRESS_LENGTH,
                                            g_trusted_name_info.addr);
        unset_payload(&g_tlv_payload);
        roll_challenge(); // prevent replays
    }
    return response_to_trusted_name(true, 0);
}

#endif  // HAVE_TRUSTED_NAME
