#include <ctype.h>
#include "trusted_name.h"
#include "network.h"  // chain_is_ethereum_compatible
#include "utils.h"    // SET_BIT
#include "read.h"
#include "challenge.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "proxy_info.h"
#include "ui_utils.h"
#include "mem_utils.h"

typedef enum { STRUCT_TYPE_TRUSTED_NAME = 0x03 } e_struct_type;

typedef enum { SIG_ALGO_SECP256K1 = 0x01 } e_sig_algo;

typedef enum {
    SLIP_44_ETHEREUM = 60,
} e_coin_type;

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

s_trusted_name_info *g_trusted_name_info = NULL;
char *g_trusted_name = NULL;

static bool matching_type(e_name_type type, uint8_t type_count, const e_name_type *types) {
    for (int i = 0; i < type_count; ++i) {
        if (type == types[i]) return true;
    }
    return false;
}

static bool matching_source(e_name_source source,
                            uint8_t source_count,
                            const e_name_source *sources) {
    for (int i = 0; i < source_count; ++i) {
        if (source == sources[i]) return true;
    }
    return false;
}

static bool matching_trusted_name(const s_trusted_name_info *trusted_name,
                                  uint8_t type_count,
                                  const e_name_type *types,
                                  uint8_t source_count,
                                  const e_name_source *sources,
                                  const uint64_t *chain_id,
                                  const uint8_t *addr) {
    const uint8_t *tmp;

    switch (trusted_name->struct_version) {
        case 1:
            if (!matching_type(TN_TYPE_ACCOUNT, type_count, types)) {
                return false;
            }
            if (!chain_is_ethereum_compatible(chain_id)) {
                return false;
            }
            break;
        case 2:
            if (!matching_type(trusted_name->name_type, type_count, types)) {
                return false;
            }
            if (!matching_source(trusted_name->name_source, source_count, sources)) {
                return false;
            }
            if (*chain_id != trusted_name->chain_id) {
                return false;
            }

            if (trusted_name->name_type == TN_TYPE_CONTRACT) {
                if ((tmp = get_implem_contract(chain_id, addr, NULL)) != NULL) {
                    addr = tmp;
                }
            }
            break;
    }
    return memcmp(addr, trusted_name->addr, ADDRESS_LENGTH) == 0;
}

/**
 * Checks if a trusted name matches the given parameters
 *
 * Always wipes the content of \ref g_trusted_name_info
 *
 * @param[in] types_count number of given trusted name types
 * @param[in] types given trusted name types
 * @param[in] chain_id given chain ID
 * @param[in] addr given address
 * @return whether there is or not
 */
const char *get_trusted_name(uint8_t type_count,
                             const e_name_type *types,
                             uint8_t source_count,
                             const e_name_source *sources,
                             const uint64_t *chain_id,
                             const uint8_t *addr) {
    const char *ret = NULL;

    if (g_trusted_name_info == NULL) {
        return NULL;
    }
    if (matching_trusted_name(g_trusted_name_info,
                              type_count,
                              types,
                              source_count,
                              sources,
                              chain_id,
                              addr)) {
        ret = g_trusted_name_info->name;
    }
    explicit_bzero(g_trusted_name_info, sizeof(s_trusted_name_info));
    return ret;
}

/**
 * Handler for tag \ref STRUCT_TYPE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_struct_type(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length != sizeof(e_struct_type)) {
        return false;
    }
    context->rcv_flags |= SET_BIT(STRUCT_TYPE_RCV_BIT);
    return (data->value[0] == STRUCT_TYPE_TRUSTED_NAME);
}

/**
 * Handler for tag \ref NOT_VALID_AFTER
 *
 * @param[in] data the tlv data
 * @param[] context the trusted name context
 * @return whether it was successful
 */
static bool handle_not_valid_after(const s_tlv_data *data, s_trusted_name_ctx *context) {
    const uint8_t app_version[] = {MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION};

    (void) context;
    if (data->length != ARRAYLEN(app_version)) {
        return false;
    }
    for (int i = 0; i < (int) ARRAYLEN(app_version); ++i) {
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
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_struct_version(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length != sizeof(context->trusted_name.struct_version)) {
        return false;
    }
    context->trusted_name.struct_version = data->value[0];
    context->rcv_flags |= SET_BIT(STRUCT_VERSION_RCV_BIT);
    return true;
}

/**
 * Handler for tag \ref CHALLENGE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_challenge(const s_tlv_data *data, s_trusted_name_ctx *context) {
    uint8_t buf[sizeof(uint32_t)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->rcv_flags |= SET_BIT(CHALLENGE_RCV_BIT);
    return (read_u32_be(buf, 0) == get_challenge());
}

/**
 * Handler for tag \ref SIGNER_KEY_ID
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_sign_key_id(const s_tlv_data *data, s_trusted_name_ctx *context) {
    // for some reason this is sent as 2 bytes
    uint16_t value;
    uint8_t buf[sizeof(value)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    value = read_u16_be(buf, 0);
    if (value > UINT8_MAX) {
        return false;
    }
    context->key_id = value;
    context->rcv_flags |= SET_BIT(SIGNER_KEY_ID_RCV_BIT);
    return true;
}

/**
 * Handler for tag \ref SIGNER_ALGO
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_sign_algo(const s_tlv_data *data, s_trusted_name_ctx *context) {
    // for some reason this is sent as 2 bytes
    uint8_t buf[sizeof(uint16_t)];

    if (data->length > sizeof(buf)) {
        return false;
    }
    buf_shrink_expand(data->value, data->length, buf, sizeof(buf));
    context->rcv_flags |= SET_BIT(SIGNER_ALGO_RCV_BIT);
    return (read_u16_be(buf, 0) == SIG_ALGO_SECP256K1);
}

/**
 * Handler for tag \ref SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_signature(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length > sizeof(context->input_sig)) {
        return false;
    }
    context->input_sig_size = data->length;
    memcpy(context->input_sig, data->value, data->length);
    context->rcv_flags |= SET_BIT(SIGNATURE_RCV_BIT);
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
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_trusted_name(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length > TRUSTED_NAME_MAX_LENGTH) {
        PRINTF("Domain name too long! (%u)\n", data->length);
        return false;
    }
    if ((context->trusted_name.struct_version == 1) ||
        (context->trusted_name.name_type == TN_TYPE_ACCOUNT)) {
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
            context->trusted_name.name[idx] = data->value[idx];
        }
    } else {
        memcpy(context->trusted_name.name, data->value, data->length);
    }
    context->trusted_name.name[data->length] = '\0';
    context->rcv_flags |= SET_BIT(TRUSTED_NAME_RCV_BIT);
    return true;
}

/**
 * Handler for tag \ref COIN_TYPE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_coin_type(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length != sizeof(e_coin_type)) {
        return false;
    }
    context->rcv_flags |= SET_BIT(COIN_TYPE_RCV_BIT);
    return (data->value[0] == SLIP_44_ETHEREUM);
}

/**
 * Handler for tag \ref ADDRESS
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_address(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length != ADDRESS_LENGTH) {
        return false;
    }
    memcpy(context->trusted_name.addr, data->value, ADDRESS_LENGTH);
    context->rcv_flags |= SET_BIT(ADDRESS_RCV_BIT);
    return true;
}

/**
 * Handler for tag \ref CHAIN_ID
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_chain_id(const s_tlv_data *data, s_trusted_name_ctx *context) {
    context->trusted_name.chain_id = u64_from_BE(data->value, data->length);
    context->rcv_flags |= SET_BIT(CHAIN_ID_RCV_BIT);
    return true;
}

/**
 * Handler for tag \ref TRUSTED_NAME_TYPE
 *
 * @param[in] data the tlv data
 * @param[in,out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_trusted_name_type(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length != sizeof(e_name_type)) {
        return false;
    }
    context->trusted_name.name_type = data->value[0];
    switch (context->trusted_name.name_type) {
        case TN_TYPE_ACCOUNT:
        case TN_TYPE_CONTRACT:
            break;
        case TN_TYPE_NFT_COLLECTION:
        case TN_TYPE_TOKEN:
        case TN_TYPE_WALLET:
        case TN_TYPE_CONTEXT_ADDRESS:
        default:
            PRINTF("Error: unsupported trusted name type (%u)!\n", context->trusted_name.name_type);
            return false;
    }
    context->rcv_flags |= SET_BIT(TRUSTED_NAME_TYPE_RCV_BIT);
    return true;
}

/**
 * Handler for tag \ref TRUSTED_NAME_SOURCE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_trusted_name_source(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length != sizeof(e_name_source)) {
        return false;
    }
    context->trusted_name.name_source = data->value[0];
    switch (context->trusted_name.name_source) {
        case TN_SOURCE_CAL:
        case TN_SOURCE_ENS:
            break;
        case TN_SOURCE_LAB:
        case TN_SOURCE_UD:
        case TN_SOURCE_FN:
        case TN_SOURCE_DNS:
        case TN_SOURCE_DYNAMIC_RESOLVER:
        default:
            PRINTF("Error: unsupported trusted name source (%u)!\n",
                   context->trusted_name.name_source);
            return false;
    }
    context->rcv_flags |= SET_BIT(TRUSTED_NAME_SOURCE_RCV_BIT);
    return true;
}

/**
 * Handler for tag \ref NFT_ID
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_nft_id(const s_tlv_data *data, s_trusted_name_ctx *context) {
    if (data->length > sizeof(context->trusted_name.nft_id)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->trusted_name.nft_id,
                      sizeof(context->trusted_name.nft_id));
    context->rcv_flags |= SET_BIT(NFT_ID_RCV_BIT);
    return true;  // unhandled for now
}

bool handle_trusted_name_struct(const s_tlv_data *data, s_trusted_name_ctx *context) {
    bool ret;

    (void) context;
    switch (data->tag) {
        case STRUCT_TYPE:
            ret = handle_struct_type(data, context);
            break;
        case STRUCT_VERSION:
            ret = handle_struct_version(data, context);
            break;
        case NOT_VALID_AFTER:
            ret = handle_not_valid_after(data, context);
            break;
        case CHALLENGE:
            ret = handle_challenge(data, context);
            break;
        case SIGNER_KEY_ID:
            ret = handle_sign_key_id(data, context);
            break;
        case SIGNER_ALGO:
            ret = handle_sign_algo(data, context);
            break;
        case SIGNATURE:
            ret = handle_signature(data, context);
            break;
        case TRUSTED_NAME:
            ret = handle_trusted_name(data, context);
            break;
        case COIN_TYPE:
            ret = handle_coin_type(data, context);
            break;
        case ADDRESS:
            ret = handle_address(data, context);
            break;
        case CHAIN_ID:
            ret = handle_chain_id(data, context);
            break;
        case TRUSTED_NAME_TYPE:
            ret = handle_trusted_name_type(data, context);
            break;
        case TRUSTED_NAME_SOURCE:
            ret = handle_trusted_name_source(data, context);
            break;
        case NFT_ID:
            ret = handle_nft_id(data, context);
            break;
        default:
            PRINTF(TLV_TAG_ERROR_MSG, data->tag);
            ret = false;
    }
    if (ret && (data->tag != SIGNATURE)) {
        hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    }
    return ret;
}

/**
 * Verify the signature context
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context the trusted name context
 * @return whether it was successful
 */
static bool verify_trusted_name_signature(const s_trusted_name_ctx *context) {
    uint8_t hash[INT256_LENGTH];
    const uint8_t *pk;
    size_t pk_size;

    switch (context->key_id) {
        case TN_KEY_ID_DOMAIN_SVC:
            pk = TRUSTED_NAME_PUB_KEY;
            pk_size = sizeof(TRUSTED_NAME_PUB_KEY);
            break;
        case TN_KEY_ID_CAL:
            pk = LEDGER_SIGNATURE_PUBLIC_KEY;
            pk_size = sizeof(LEDGER_SIGNATURE_PUBLIC_KEY);
            break;
        default:
            PRINTF("Error: Unknown metadata key ID %u\n", context->key_id);
            return false;
    }

    if (cx_hash_no_throw((cx_hash_t *) &context->hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH) !=
        CX_OK) {
        return false;
    }

    if (check_signature_with_pubkey("Trusted Name",
                                    hash,
                                    sizeof(hash),
                                    pk,
                                    pk_size,
                                    CERTIFICATE_PUBLIC_KEY_USAGE_TRUSTED_NAME,
                                    (uint8_t *) (context->input_sig),
                                    context->input_sig_size) != CX_OK) {
        return false;
    }
    return true;
}

/**
 * Verify the validity of the received trusted struct
 *
 * @param[in] context the trusted name context
 * @return whether the struct is valid
 */
bool verify_trusted_name_struct(const s_trusted_name_ctx *context) {
    uint32_t required_flags;

    if (!(SET_BIT(STRUCT_VERSION_RCV_BIT) & context->rcv_flags)) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }
    required_flags = SET_BIT(STRUCT_TYPE_RCV_BIT) | SET_BIT(STRUCT_VERSION_RCV_BIT) |
                     SET_BIT(SIGNER_KEY_ID_RCV_BIT) | SET_BIT(SIGNER_ALGO_RCV_BIT) |
                     SET_BIT(SIGNATURE_RCV_BIT) | SET_BIT(TRUSTED_NAME_RCV_BIT) |
                     SET_BIT(ADDRESS_RCV_BIT);
    switch (context->trusted_name.struct_version) {
        case 1:
            required_flags |= SET_BIT(CHALLENGE_RCV_BIT) | SET_BIT(COIN_TYPE_RCV_BIT);
            if ((context->rcv_flags & required_flags) != required_flags) {
                return false;
            }
            break;
        case 2:
            required_flags |= SET_BIT(CHAIN_ID_RCV_BIT) | SET_BIT(TRUSTED_NAME_TYPE_RCV_BIT) |
                              SET_BIT(TRUSTED_NAME_SOURCE_RCV_BIT);
            if ((context->rcv_flags & required_flags) != required_flags) {
                return false;
            }
            switch (context->trusted_name.name_type) {
                case TN_TYPE_ACCOUNT:
                    if (context->trusted_name.name_source == TN_SOURCE_CAL) {
                        PRINTF("Error: cannot accept an account name from the CAL!\n");
                        return false;
                    }
                    if (!(context->rcv_flags & SET_BIT(CHALLENGE_RCV_BIT))) {
                        PRINTF("Error: trusted account name requires a challenge!\n");
                        return false;
                    }
                    break;
                case TN_TYPE_CONTRACT:
                    if (context->trusted_name.name_source != TN_SOURCE_CAL) {
                        PRINTF("Error: cannot accept a contract name from given source (%u)!\n",
                               context->trusted_name.name_source);
                        return false;
                    }
                    break;
                default:
                    return false;
            }
            break;
        default:
            PRINTF("Error: unsupported trusted name struct version (%u) !\n",
                   context->trusted_name.struct_version);
            return false;
    }

    if (!verify_trusted_name_signature(context)) {
        return false;
    }

    // Allocate the Trusted Name buffer
    if (mem_buffer_allocate((void **) &g_trusted_name_info, sizeof(s_trusted_name_info)) == false) {
        PRINTF("Memory allocation failed for Trusted Name\n");
        return false;
    }

    memcpy(g_trusted_name_info, &context->trusted_name, sizeof(s_trusted_name_info));

    PRINTF("Registered : %s => %.*h\n",
           g_trusted_name_info->name,
           ADDRESS_LENGTH,
           g_trusted_name_info->addr);
    return true;
}
