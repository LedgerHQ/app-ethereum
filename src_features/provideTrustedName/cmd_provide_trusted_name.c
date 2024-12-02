#ifdef HAVE_TRUSTED_NAME

#include <os.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "common_utils.h"  // ARRAY_SIZE
#include "apdu_constants.h"
#include "trusted_name.h"
#include "challenge.h"
#include "mem.h"
#include "hash_bytes.h"
#include "network.h"
#include "public_keys.h"
#ifdef HAVE_LEDGER_PKI
#include "os_pki.h"
#endif

#define STRUCT_TYPE_TRUSTED_NAME 0x03
#define ALGO_SECP256K1           1

#define SLIP_44_ETHEREUM 60

#define DER_LONG_FORM_FLAG        0x80  // 8th bit set
#define DER_FIRST_BYTE_VALUE_MASK 0x7f

typedef enum { TLV_TAG, TLV_LENGTH, TLV_VALUE } e_tlv_step;

// This enum needs to be ordered the same way as the e_tlv_tag one !
typedef enum {
    STRUCT_TYPE_RCV_BIT = 0,
    STRUCT_VERSION_RCV_BIT,
    NOT_VALID_AFTER_RCV_BIT,
    CHALLENGE_RCV_BIT,
    SIGNER_KEY_ID_RCV_BIT,
    SIGNER_ALGO_RCV_BIT,
    SIGNATURE_RCV_BIT,
    TRUSTED_NAME_RCV_BIT,
    COIN_TYPE_RCV_BIT,
    ADDRESS_RCV_BIT,
    CHAIN_ID_RCV_BIT,
    TRUSTED_NAME_TYPE_RCV_BIT,
    TRUSTED_NAME_SOURCE_RCV_BIT,
    NFT_ID_RCV_BIT,
} e_tlv_rcv_bit;

#define RCV_FLAG(a) (1 << a)

typedef enum {
    STRUCT_TYPE = 0x01,
    STRUCT_VERSION = 0x02,
    NOT_VALID_AFTER = 0x10,
    CHALLENGE = 0x12,
    SIGNER_KEY_ID = 0x13,
    SIGNER_ALGO = 0x14,
    SIGNATURE = 0x15,
    TRUSTED_NAME = 0x20,
    COIN_TYPE = 0x21,
    ADDRESS = 0x22,
    CHAIN_ID = 0x23,
    TRUSTED_NAME_TYPE = 0x70,
    TRUSTED_NAME_SOURCE = 0x71,
    NFT_ID = 0x72,
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
    uint32_t rcv_flags;
    bool valid;
    uint8_t struct_version;
    char *name;
    uint8_t addr[ADDRESS_LENGTH];
    uint64_t chain_id;
    e_name_type name_type;
    e_name_source name_source;
    uint8_t nft_id[INT256_LENGTH];
} s_trusted_name_info;

typedef struct {
    e_key_id key_id;
    uint8_t input_sig_size;
    const uint8_t *input_sig;
    cx_sha256_t hash_ctx;
} s_sig_ctx;

typedef bool(t_tlv_handler)(const s_tlv_data *data,
                            s_trusted_name_info *trusted_name_info,
                            s_sig_ctx *sig_ctx);

typedef struct {
    e_tlv_tag tag;
    t_tlv_handler *func;
    e_tlv_rcv_bit rcv_bit;
} s_tlv_handler;

static s_tlv_payload g_tlv_payload = {0};
static s_trusted_name_info g_trusted_name_info = {0};
char g_trusted_name[TRUSTED_NAME_MAX_LENGTH + 1];

/**
 * Checks if a trusted name matches the given parameters
 *
 * Does not care about the trusted name source for now.
 * Always wipes the content of \ref g_trusted_name_info
 *
 * @param[in] types_count number of given trusted name types
 * @param[in] types given trusted name types
 * @param[in] chain_id given chain ID
 * @param[in] addr given address
 * @return whether there is or not
 */
bool has_trusted_name(uint8_t types_count,
                      const e_name_type *types,
                      const uint64_t *chain_id,
                      const uint8_t *addr) {
    bool ret = false;

    if (g_trusted_name_info.rcv_flags != 0) {
        for (int i = 0; i < types_count; ++i) {
            switch (g_trusted_name_info.struct_version) {
                case 1:
                    if (types[i] == TYPE_ACCOUNT) {
                        // Check if chain ID is known to be Ethereum-compatible (same derivation
                        // path)
                        if ((chain_is_ethereum_compatible(chain_id)) &&
                            (memcmp(addr, g_trusted_name_info.addr, ADDRESS_LENGTH) == 0)) {
                            ret = true;
                        }
                    }
                    break;
                case 2:
                    if (types[i] == g_trusted_name_info.name_type) {
                        if (*chain_id == g_trusted_name_info.chain_id) {
                            ret = true;
                        }
                    }
                    break;
                default:
                    ret = false;
            }
            if (ret) break;
        }
        explicit_bzero(&g_trusted_name_info, sizeof(g_trusted_name_info));
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
 * Handler for tag \ref STRUCT_TYPE
 *
 * @param[in] data the tlv data
 * @param[] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_struct_type(const s_tlv_data *data,
                               s_trusted_name_info *trusted_name_info,
                               s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) trusted_name_info;
    (void) sig_ctx;
    if (!get_uint_from_data(data, &value)) {
        return false;
    }
    return (value == STRUCT_TYPE_TRUSTED_NAME);
}

/**
 * Handler for tag \ref NOT_VALID_AFTER
 *
 * @param[in] data the tlv data
 * @param[] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_not_valid_after(const s_tlv_data *data,
                                   s_trusted_name_info *trusted_name_info,
                                   s_sig_ctx *sig_ctx) {
    const uint8_t app_version[] = {MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION};
    int i = 0;

    (void) trusted_name_info;
    (void) sig_ctx;
    if (data->length != ARRAYLEN(app_version)) {
        return false;
    }

    for (; i < (int) ARRAYLEN(app_version); ++i) {
        if (data->value[i] > app_version[i]) {
            break;
        } else if (data->value[i] < app_version[i]) {
            PRINTF("Expired trusted name : %u.%u.%u < %u.%u.%u\n",
                   data->value[0],
                   data->value[1],
                   data->value[2],
                   app_version[0],
                   app_version[1],
                   app_version[2]);
            return false;
        }
    }
    return true;
}

/**
 * Handler for tag \ref STRUCT_VERSION
 *
 * @param[in] data the tlv data
 * @param[out] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_struct_version(const s_tlv_data *data,
                                  s_trusted_name_info *trusted_name_info,
                                  s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) sig_ctx;
    if (!get_uint_from_data(data, &value) || (value > UINT8_MAX)) {
        return false;
    }
    trusted_name_info->struct_version = value;
    return true;
}

/**
 * Handler for tag \ref CHALLENGE
 *
 * @param[in] data the tlv data
 * @param[] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_challenge(const s_tlv_data *data,
                             s_trusted_name_info *trusted_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;
    (void) trusted_name_info;
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
 * @param[] trusted_name_info the trusted name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_sign_key_id(const s_tlv_data *data,
                               s_trusted_name_info *trusted_name_info,
                               s_sig_ctx *sig_ctx) {
    uint32_t value;
    (void) trusted_name_info;

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
 * @param[] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_sign_algo(const s_tlv_data *data,
                             s_trusted_name_info *trusted_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) trusted_name_info;
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
 * @param[] trusted_name_info the trusted name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_signature(const s_tlv_data *data,
                             s_trusted_name_info *trusted_name_info,
                             s_sig_ctx *sig_ctx) {
    (void) trusted_name_info;
    sig_ctx->input_sig_size = data->length;
    sig_ctx->input_sig = data->value;
    return true;
}

/**
 * Tests if the given account name character is valid (in our subset of allowed characters)
 *
 * @param[in] c given character
 * @return whether the character is valid
 */
static bool is_valid_account_character(char c) {
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
 * Handler for tag \ref TRUSTED_NAME
 *
 * @param[in] data the tlv data
 * @param[out] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_trusted_name(const s_tlv_data *data,
                                s_trusted_name_info *trusted_name_info,
                                s_sig_ctx *sig_ctx) {
    (void) sig_ctx;
    if (data->length > TRUSTED_NAME_MAX_LENGTH) {
        PRINTF("Domain name too long! (%u)\n", data->length);
        return false;
    }
    if ((trusted_name_info->struct_version == 1) ||
        (trusted_name_info->name_type == TYPE_ACCOUNT)) {
        // TODO: Remove once other domain name providers are supported
        if ((data->length < 5) ||
            (strncmp(".eth", (char *) &data->value[data->length - 4], 4) != 0)) {
            PRINTF("Unexpected TLD!\n");
            return false;
        }
        for (int idx = 0; idx < data->length; ++idx) {
            if (!is_valid_account_character(data->value[idx])) {
                PRINTF("Domain name contains non-allowed character! (0x%x)\n", data->value[idx]);
                return false;
            }
            trusted_name_info->name[idx] = data->value[idx];
        }
    } else {
        memcpy(trusted_name_info->name, data->value, data->length);
    }
    trusted_name_info->name[data->length] = '\0';
    return true;
}

/**
 * Handler for tag \ref COIN_TYPE
 *
 * @param[in] data the tlv data
 * @param[] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_coin_type(const s_tlv_data *data,
                             s_trusted_name_info *trusted_name_info,
                             s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) trusted_name_info;
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
 * @param[out] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_address(const s_tlv_data *data,
                           s_trusted_name_info *trusted_name_info,
                           s_sig_ctx *sig_ctx) {
    (void) sig_ctx;
    if (data->length != ADDRESS_LENGTH) {
        return false;
    }
    memcpy(trusted_name_info->addr, data->value, ADDRESS_LENGTH);
    return true;
}

/**
 * Handler for tag \ref CHAIN_ID
 *
 * @param[in] data the tlv data
 * @param[out] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_chain_id(const s_tlv_data *data,
                            s_trusted_name_info *trusted_name_info,
                            s_sig_ctx *sig_ctx) {
    (void) sig_ctx;
    trusted_name_info->chain_id = u64_from_BE(data->value, data->length);
    return true;
}

/**
 * Handler for tag \ref TRUSTED_NAME_TYPE
 *
 * @param[in] data the tlv data
 * @param[out] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_trusted_name_type(const s_tlv_data *data,
                                     s_trusted_name_info *trusted_name_info,
                                     s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) trusted_name_info;
    (void) sig_ctx;
    if (!get_uint_from_data(data, &value) || (value > UINT8_MAX)) {
        return false;
    }
    switch (value) {
        case TYPE_ACCOUNT:
        case TYPE_CONTRACT:
            break;
        case TYPE_NFT:
        default:
            PRINTF("Error: unsupported trusted name type (%u)!\n", value);
            return false;
    }
    trusted_name_info->name_type = value;
    return true;
}

/**
 * Handler for tag \ref TRUSTED_NAME_SOURCE
 *
 * @param[in] data the tlv data
 * @param[out] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_trusted_name_source(const s_tlv_data *data,
                                       s_trusted_name_info *trusted_name_info,
                                       s_sig_ctx *sig_ctx) {
    uint32_t value;

    (void) trusted_name_info;
    (void) sig_ctx;
    if (!get_uint_from_data(data, &value) || (value > UINT8_MAX)) {
        return false;
    }
    switch (value) {
        case SOURCE_CAL:
        case SOURCE_ENS:
            break;
        case SOURCE_LAB:
        case SOURCE_UD:
        case SOURCE_FN:
        case SOURCE_DNS:
        default:
            PRINTF("Error: unsupported trusted name source (%u)!\n", value);
            return false;
    }
    trusted_name_info->name_source = value;
    return true;
}

/**
 * Handler for tag \ref NFT_ID
 *
 * @param[in] data the tlv data
 * @param[out] trusted_name_info the trusted name information
 * @param[] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_nft_id(const s_tlv_data *data,
                          s_trusted_name_info *trusted_name_info,
                          s_sig_ctx *sig_ctx) {
    size_t diff;

    (void) trusted_name_info;
    (void) sig_ctx;
    if (data->length > sizeof(trusted_name_info->nft_id)) {
        return false;
    }
    diff = sizeof(trusted_name_info->nft_id) - data->length;
    memmove(trusted_name_info->nft_id + diff, data->value, data->length);
    explicit_bzero(trusted_name_info->nft_id, diff);
    return true;  // unhandled for now
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
    cx_err_t error = CX_INTERNAL_ERROR;
#ifdef HAVE_TRUSTED_NAME_TEST_KEY
    e_key_id valid_key_id = KEY_ID_TEST;
#else
    e_key_id valid_key_id = KEY_ID_PROD;
#endif
    bool ret_code = false;

    if (sig_ctx->key_id != valid_key_id) {
        PRINTF("Error: Unknown metadata key ID %u\n", sig_ctx->key_id);
        return false;
    }

    CX_CHECK(
        cx_hash_no_throw((cx_hash_t *) &sig_ctx->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH));

    CX_CHECK(check_signature_with_pubkey("Domain Name",
                                         hash,
                                         sizeof(hash),
                                         TRUSTED_NAME_PUB_KEY,
                                         sizeof(TRUSTED_NAME_PUB_KEY),
#ifdef HAVE_LEDGER_PKI
                                         CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
#endif
                                         (uint8_t *) (sig_ctx->input_sig),
                                         sig_ctx->input_sig_size));

    ret_code = true;
end:
    return ret_code;
}

/**
 * Calls the proper handler for the given TLV data
 *
 * Checks if there is a proper handler function for the given TLV tag and then calls it
 *
 * @param[in] handlers list of tag / handler function pairs
 * @param[in] handler_count number of handlers
 * @param[in] data the TLV data
 * @param[out] trusted_name_info the trusted name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool handle_tlv_data(s_tlv_handler *handlers,
                            int handler_count,
                            const s_tlv_data *data,
                            s_trusted_name_info *trusted_name_info,
                            s_sig_ctx *sig_ctx) {
    t_tlv_handler *fptr;

    // check if a handler exists for this tag
    for (int idx = 0; idx < handler_count; ++idx) {
        if (handlers[idx].tag == data->tag) {
            trusted_name_info->rcv_flags |= RCV_FLAG(handlers[idx].rcv_bit);
            fptr = PIC(handlers[idx].func);
            if (!(*fptr)(data, trusted_name_info, sig_ctx)) {
                PRINTF("Error while handling tag 0x%x\n", handlers[idx].tag);
                return false;
            }
            break;
        }
    }
    return true;
}

/**
 * Verify the validity of the received trusted struct
 *
 * @param[in] trusted_name_info the trusted name information
 * @return whether the struct is valid
 */
static bool verify_struct(const s_trusted_name_info *trusted_name_info) {
    uint32_t required_flags;

    if (!(RCV_FLAG(STRUCT_VERSION_RCV_BIT) & trusted_name_info->rcv_flags)) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }
    required_flags = RCV_FLAG(STRUCT_TYPE_RCV_BIT) | RCV_FLAG(STRUCT_VERSION_RCV_BIT) |
                     RCV_FLAG(SIGNER_KEY_ID_RCV_BIT) | RCV_FLAG(SIGNER_ALGO_RCV_BIT) |
                     RCV_FLAG(SIGNATURE_RCV_BIT) | RCV_FLAG(TRUSTED_NAME_RCV_BIT) |
                     RCV_FLAG(ADDRESS_RCV_BIT);
    switch (trusted_name_info->struct_version) {
        case 1:
            required_flags |= RCV_FLAG(CHALLENGE_RCV_BIT) | RCV_FLAG(COIN_TYPE_RCV_BIT);
            if ((trusted_name_info->rcv_flags & required_flags) != required_flags) {
                return false;
            }
            break;
        case 2:
            required_flags |= RCV_FLAG(CHAIN_ID_RCV_BIT) | RCV_FLAG(TRUSTED_NAME_TYPE_RCV_BIT) |
                              RCV_FLAG(TRUSTED_NAME_SOURCE_RCV_BIT);
            if ((trusted_name_info->rcv_flags & required_flags) != required_flags) {
                return false;
            }
            switch (trusted_name_info->name_type) {
                case TYPE_ACCOUNT:
                    if (trusted_name_info->name_source == SOURCE_CAL) {
                        PRINTF("Error: cannot accept an account name from the CAL!\n");
                        return false;
                    }
                    if (!(trusted_name_info->rcv_flags & RCV_FLAG(CHALLENGE_RCV_BIT))) {
                        PRINTF("Error: trusted account name requires a challenge!\n");
                        return false;
                    }
                    break;
                case TYPE_CONTRACT:
                    if (trusted_name_info->name_source != SOURCE_CAL) {
                        PRINTF("Error: cannot accept a contract name from given source (%u)!\n",
                               trusted_name_info->name_source);
                        return false;
                    }
                    break;
                default:
                    return false;
            }
            break;
        default:
            PRINTF("Error: unsupported trusted name struct version (%u) !\n",
                   trusted_name_info->struct_version);
            return false;
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
        } else {
            if (tmp_value <= UINT8_MAX) {
                *value = tmp_value;
                ret = true;
            } else {
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
 * @param[out] trusted_name_info the trusted name information
 * @param[out] sig_ctx the signature context
 * @return whether it was successful
 */
static bool parse_tlv(const s_tlv_payload *payload,
                      s_trusted_name_info *trusted_name_info,
                      s_sig_ctx *sig_ctx) {
    s_tlv_handler handlers[] = {
        {.tag = STRUCT_TYPE, .func = &handle_struct_type},
        {.tag = STRUCT_VERSION, .func = &handle_struct_version},
        {.tag = NOT_VALID_AFTER, .func = &handle_not_valid_after},
        {.tag = CHALLENGE, .func = &handle_challenge},
        {.tag = SIGNER_KEY_ID, .func = &handle_sign_key_id},
        {.tag = SIGNER_ALGO, .func = &handle_sign_algo},
        {.tag = SIGNATURE, .func = &handle_signature},
        {.tag = TRUSTED_NAME, .func = &handle_trusted_name},
        {.tag = COIN_TYPE, .func = &handle_coin_type},
        {.tag = ADDRESS, .func = &handle_address},
        {.tag = CHAIN_ID, .func = &handle_chain_id},
        {.tag = TRUSTED_NAME_TYPE, .func = &handle_trusted_name_type},
        {.tag = TRUSTED_NAME_SOURCE, .func = &handle_trusted_name_source},
        {.tag = NFT_ID, .func = &handle_nft_id},
    };
    e_tlv_step step = TLV_TAG;
    s_tlv_data data;
    size_t offset = 0;
    size_t tag_start_off;

    for (size_t i = 0; i < ARRAYLEN(handlers); ++i) handlers[i].rcv_bit = i;
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
                if ((offset + data.length) > payload->size) {
                    PRINTF("Error: value would go beyond the TLV payload!\n");
                    return false;
                }
                data.value = &payload->buf[offset];
                if (!handle_tlv_data(handlers,
                                     ARRAY_SIZE(handlers),
                                     &data,
                                     trusted_name_info,
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
    if (step != TLV_TAG) {
        PRINTF("Error: unexpected data at the end of the TLV payload!\n");
        return false;
    }
    return verify_struct(trusted_name_info);
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

static bool handle_first_chunk(const uint8_t **data,
                               uint8_t *length,
                               s_tlv_payload *payload,
                               uint16_t *sw) {
    // check if no payload is already in memory
    if (payload->buf != NULL) {
        free_payload(payload);
        *sw = APDU_RESPONSE_INVALID_P1_P2;
        return false;
    }

    // check if we at least get the size
    if (*length < sizeof(payload->expected_size)) {
        *sw = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    if (!alloc_payload(payload, U2BE(*data, 0))) {
        *sw = APDU_RESPONSE_INSUFFICIENT_MEMORY;
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
 * @param[in] data APDU payload
 * @param[in] length payload size
 */
uint16_t handle_provide_trusted_name(uint8_t p1, const uint8_t *data, uint8_t length) {
    s_sig_ctx sig_ctx;
    uint16_t sw = APDU_NO_RESPONSE;

    if (p1 == P1_FIRST_CHUNK) {
        if (!handle_first_chunk(&data, &length, &g_tlv_payload, &sw)) {
            return sw;
        }
    } else {
        // check if a payload is already in memory
        if (g_tlv_payload.buf == NULL) {
            return APDU_RESPONSE_INVALID_P1_P2;
        }
    }

    if ((g_tlv_payload.size + length) > g_tlv_payload.expected_size) {
        free_payload(&g_tlv_payload);
        PRINTF("TLV payload size mismatch!\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    // feed into tlv payload
    memcpy(g_tlv_payload.buf + g_tlv_payload.size, data, length);
    g_tlv_payload.size += length;

    // everything has been received
    if (g_tlv_payload.size == g_tlv_payload.expected_size) {
        g_trusted_name_info.name = g_trusted_name;
        if (!parse_tlv(&g_tlv_payload, &g_trusted_name_info, &sig_ctx) ||
            !verify_signature(&sig_ctx)) {
            free_payload(&g_tlv_payload);
            roll_challenge();  // prevent brute-force guesses
            g_trusted_name_info.rcv_flags = 0;
            return APDU_RESPONSE_INVALID_DATA;
        }
        PRINTF("Registered : %s => %.*h\n",
               g_trusted_name_info.name,
               ADDRESS_LENGTH,
               g_trusted_name_info.addr);
        free_payload(&g_tlv_payload);
        roll_challenge();  // prevent replays
    }
    return APDU_RESPONSE_OK;
}

#endif  // HAVE_TRUSTED_NAME
