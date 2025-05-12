#ifdef HAVE_TRUSTED_NAME

#include <ctype.h>
#include "trusted_name.h"
#include "network.h"  // chain_is_ethereum_compatible
#include "utils.h"    // SET_BIT
#include "read.h"
#include "mem.h"
#include "challenge.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "proxy_info.h"
#include "tlv_library.h"

typedef enum { STRUCT_TYPE_TRUSTED_NAME = 0x03 } e_struct_type;

typedef enum { SIG_ALGO_SECP256K1 = 0x01 } e_sig_algo;

typedef enum { TN_KEY_ID_DOMAIN_SVC = 0x07, TN_KEY_ID_CAL = 0x09 } e_tn_key_id;

typedef enum {
    SLIP_44_ETHEREUM = 60,
} e_coin_type;

typedef struct {
    bool valid;
    uint8_t struct_version;
    char *name;
    uint8_t addr[ADDRESS_LENGTH];
    uint64_t chain_id;
    e_name_type name_type;
    e_name_source name_source;
#ifdef HAVE_NFT_SUPPORT
    uint8_t nft_id[INT256_LENGTH];
#endif
} s_trusted_name_info;

typedef struct {
    TLV_reception_t received_tags;
    s_trusted_name_info trusted_name;
    e_tn_key_id key_id;
    buffer_t input_sig;
    cx_sha256_t hash_ctx;
} s_trusted_name_ctx;

static s_trusted_name_info g_trusted_name_info = {0};
char g_trusted_name[TRUSTED_NAME_MAX_LENGTH + 1];

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

    if (matching_trusted_name(&g_trusted_name_info,
                              type_count,
                              types,
                              source_count,
                              sources,
                              chain_id,
                              addr)) {
        ret = g_trusted_name_info.name;
    }
    explicit_bzero(&g_trusted_name_info, sizeof(g_trusted_name_info));
    return ret;
}

static bool handle_struct_type(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    uint8_t type;
    if (!get_uint8_t_from_tlv_data(data, &type)) {
        return false;
    }
    return (type == STRUCT_TYPE_TRUSTED_NAME);
}

static bool handle_struct_version(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    return get_uint8_t_from_tlv_data(data, &context->trusted_name.struct_version);
}

static bool handle_not_valid_after(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    const uint8_t app_version[] = {MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION};
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

static bool handle_challenge(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    uint32_t challenge;
    if (!get_uint32_t_from_tlv_data(data, &challenge)) {
        return false;
    }
    return (challenge == get_challenge());
}

static bool handle_sign_key_id(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    return get_uint8_t_from_tlv_data(data, &context->key_id);
}

static bool handle_sign_algo(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    uint8_t algo;
    if (!get_uint8_t_from_tlv_data(data, &algo)) {
        return false;
    }
    return (algo == SIG_ALGO_SECP256K1);
}

static bool handle_signature(const tlv_data_t *data, s_trusted_name_ctx *context) {
    // 0 copy because the signature can be dropped after check
    return get_buffer_from_tlv_data(data, &context->input_sig, 1, ECDSA_SIGNATURE_MAX_LENGTH);
}

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

static bool handle_trusted_name(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
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
    return true;
}

static bool handle_coin_type(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    uint8_t coin_type;
    if (!get_uint8_t_from_tlv_data(data, &coin_type)) {
        return false;
    }
    return (coin_type == SLIP_44_ETHEREUM);
}

static bool handle_address(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    if (data->length != ADDRESS_LENGTH) {
        return false;
    }
    memcpy(context->trusted_name.addr, data->value, ADDRESS_LENGTH);
    return true;
}

static bool handle_chain_id(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    context->trusted_name.chain_id = u64_from_BE(data->value, data->length);
    return true;
}

static bool handle_trusted_name_type(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
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
    return true;
}

static bool handle_trusted_name_source(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
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
    return true;
}

#ifdef HAVE_NFT_SUPPORT
static bool handle_nft_id(const tlv_data_t *data, s_trusted_name_ctx *context) {
    hash_nbytes(data->raw, data->raw_size, (cx_hash_t *) &context->hash_ctx);
    if (data->length > sizeof(context->trusted_name.nft_id)) {
        return false;
    }
    buf_shrink_expand(data->value,
                      data->length,
                      context->trusted_name.nft_id,
                      sizeof(context->trusted_name.nft_id));
    return true;  // unhandled for now
}
#endif

/**
 * Verify the signature context
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context the trusted name context
 * @return whether it was successful
 */
static bool verify_trusted_name_signature(const s_trusted_name_ctx *context) {
    uint8_t hash[CX_SHA256_SIZE];
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

    CX_ASSERT(cx_hash_final((cx_hash_t *) &context->hash_ctx, hash));

    if (check_signature_with_pubkey("Trusted Name",
                                    hash,
                                    sizeof(hash),
                                    pk,
                                    pk_size,
#ifdef HAVE_LEDGER_PKI
                                    CERTIFICATE_PUBLIC_KEY_USAGE_TRUSTED_NAME,
#endif
                                    context->input_sig.ptr,
                                    context->input_sig.size) != CX_OK) {
        return false;
    }
    return true;
}

#ifdef HAVE_NFT_SUPPORT
#define MAYBE_NFT_TAG(X) X(0x72, NFT_ID, handle_nft_id, ENFORCE_UNIQUE_TAG)
#else
#define MAYBE_NFT_TAG(X)
#endif

// clang-format off
#define TLV_TAGS(X)                                                              \
    X(0x01, STRUCT_TYPE,         handle_struct_type,         ENFORCE_UNIQUE_TAG) \
    X(0x02, STRUCT_VERSION,      handle_struct_version,      ENFORCE_UNIQUE_TAG) \
    X(0x10, NOT_VALID_AFTER,     handle_not_valid_after,     ENFORCE_UNIQUE_TAG) \
    X(0x12, CHALLENGE,           handle_challenge,           ENFORCE_UNIQUE_TAG) \
    X(0x13, SIGNER_KEY_ID,       handle_sign_key_id,         ENFORCE_UNIQUE_TAG) \
    X(0x14, SIGNER_ALGO,         handle_sign_algo,           ENFORCE_UNIQUE_TAG) \
    X(0x15, SIGNATURE,           handle_signature,           ENFORCE_UNIQUE_TAG) \
    X(0x20, TRUSTED_NAME,        handle_trusted_name,        ENFORCE_UNIQUE_TAG) \
    X(0x21, COIN_TYPE,           handle_coin_type,           ENFORCE_UNIQUE_TAG) \
    X(0x22, ADDRESS,             handle_address,             ENFORCE_UNIQUE_TAG) \
    X(0x23, CHAIN_ID,            handle_chain_id,            ENFORCE_UNIQUE_TAG) \
    X(0x70, TRUSTED_NAME_TYPE,   handle_trusted_name_type,   ENFORCE_UNIQUE_TAG) \
    X(0x71, TRUSTED_NAME_SOURCE, handle_trusted_name_source, ENFORCE_UNIQUE_TAG) \
    MAYBE_NFT_TAG(X)
// clang-format on

DEFINE_TLV_PARSER(TLV_TAGS, parse_tlv_trusted_name)

/**
 * Verify the validity of the received trusted struct
 *
 * @param[in] context the trusted name context
 * @return whether the struct is valid
 */
static bool verify_trusted_name_struct(const s_trusted_name_ctx *context) {
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                 STRUCT_VERSION,
                                 STRUCT_TYPE,
                                 STRUCT_VERSION,
                                 SIGNER_KEY_ID,
                                 SIGNER_ALGO,
                                 SIGNATURE,
                                 TRUSTED_NAME,
                                 ADDRESS)) {
        PRINTF("Error: no struct version specified!\n");
        return false;
    }
    switch (context->trusted_name.struct_version) {
        case 1:
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                         CHALLENGE,
                                         COIN_TYPE)) {
                PRINTF("Error: missing required fields in struct version 1\n");
                return false;
            }
            break;
        case 2:
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                         CHAIN_ID,
                                         TRUSTED_NAME_TYPE,
                                         TRUSTED_NAME_SOURCE)) {
                PRINTF("Error: missing required fields in struct version 2\n");
                return false;
            }
            switch (context->trusted_name.name_type) {
                case TN_TYPE_ACCOUNT:
                    if (context->trusted_name.name_source == TN_SOURCE_CAL) {
                        PRINTF("Error: cannot accept an account name from the CAL!\n");
                        return false;
                    }
                    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, CHALLENGE)) {
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

    memcpy(&g_trusted_name_info, &context->trusted_name, sizeof(g_trusted_name_info));

    PRINTF("Registered : %s => %.*h\n",
           g_trusted_name_info.name,
           ADDRESS_LENGTH,
           g_trusted_name_info.addr);
    return true;
}

bool handle_tlv_trusted_name_payload(const uint8_t *payload, uint16_t size, bool to_free) {
    s_trusted_name_ctx ctx = {0};
    bool parsing_ret;
    buffer_t payload_buffer = {.ptr = payload, .size = size};

    ctx.trusted_name.name = g_trusted_name;
    cx_sha256_init(&ctx.hash_ctx);
    parsing_ret = parse_tlv_trusted_name(&payload_buffer, &ctx, &ctx.received_tags);
    if (to_free) mem_dealloc(size);
    if (!parsing_ret || !verify_trusted_name_struct(&ctx)) {
        roll_challenge();  // prevent brute-force guesses
        return false;
    }
    roll_challenge();  // prevent replays
    return true;
}

#endif  // HAVE_TRUSTED_NAME
