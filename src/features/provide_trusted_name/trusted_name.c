#include <ctype.h>
#include "buffer.h"
#include "trusted_name.h"
#include "network.h"  // chain_is_ethereum_compatible
#include "utils.h"    // SET_BIT
#include "challenge.h"
#include "hash_bytes.h"
#include "public_keys.h"
#include "proxy_info.h"
#include "ui_utils.h"
#include "mem.h"
#include "getPublicKey.h"
#include "tlv_apdu.h"

#define STRUCT_VERSION_1 0x01
#define STRUCT_VERSION_2 0x02

#define STRUCT_TYPE_TRUSTED_NAME 0x03
#define SIG_ALGO_SECP256K1       0x01
#define SLIP_44_ETHEREUM         60

static s_trusted_name *g_trusted_name_list = NULL;

static void delete_trusted_name(s_trusted_name *node) {
    app_mem_free(node);
}

void trusted_name_cleanup(void) {
    flist_clear((flist_node_t **) &g_trusted_name_list, (f_list_node_del) &delete_trusted_name);
}

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

static bool matching_trusted_name(const s_trusted_name *trusted_name,
                                  uint8_t type_count,
                                  const e_name_type *types,
                                  uint8_t source_count,
                                  const e_name_source *sources,
                                  const uint64_t *chain_id,
                                  const uint8_t *addr) {
    const uint8_t *tmp;

    switch (trusted_name->struct_version) {
        case STRUCT_VERSION_1:
            if (!matching_type(TN_TYPE_ACCOUNT, type_count, types)) {
                return false;
            }
            if (!chain_is_ethereum_compatible(chain_id)) {
                return false;
            }
            break;
        case STRUCT_VERSION_2:
            if (!matching_type(trusted_name->name_type, type_count, types)) {
                return false;
            }
            if (!matching_source(trusted_name->name_source, source_count, sources)) {
                return false;
            }
            if (*chain_id != trusted_name->chain_id) {
                return false;
            }

            if ((trusted_name->name_type == TN_TYPE_CONTRACT) ||
                (trusted_name->name_type == TN_TYPE_TOKEN)) {
                if ((tmp = get_implem_contract(chain_id, addr, NULL)) != NULL) {
                    addr = tmp;
                }
            }
            break;
    }
    return memcmp(addr, trusted_name->addr, ADDRESS_LENGTH) == 0;
}

/**
 * Get a trusted name that matches the given parameters
 *
 * @param[in] types_count number of given trusted name types
 * @param[in] types given trusted name types
 * @param[in] chain_id given chain ID
 * @param[in] addr given address
 * @return the matching trusted name if found, \ref NULL otherwise
 */
const s_trusted_name *get_trusted_name(uint8_t type_count,
                                       const e_name_type *types,
                                       uint8_t source_count,
                                       const e_name_source *sources,
                                       const uint64_t *chain_id,
                                       const uint8_t *addr) {
    for (s_trusted_name *tmp = g_trusted_name_list; tmp != NULL;
         tmp = (s_trusted_name *) ((flist_node_t *) tmp)->next) {
        if (matching_trusted_name(tmp, type_count, types, source_count, sources, chain_id, addr)) {
            return tmp;
        }
    }
    return NULL;
}

/**
 * Handler for tag STRUCTURE_TYPE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_struct_type(const tlv_data_t *data, s_trusted_name_ctx *context) {
    UNUSED(context);
    return tlv_check_struct_type(data, STRUCT_TYPE_TRUSTED_NAME);
}

/**
 * Handler for tag STRUCTURE_VERSION
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_struct_version(const tlv_data_t *data, s_trusted_name_ctx *context) {
    uint8_t value = 0;
    if (!get_uint8_t_from_tlv_data(data, &value)) {
        PRINTF("STRUCTURE_VERSION: failed to extract\n");
        return false;
    }
    switch (value) {
        case STRUCT_VERSION_1:
        case STRUCT_VERSION_2:
            break;
        default:
            PRINTF("Unsupported STRUCTURE_VERSION: %u\n", value);
            return false;
    }
    context->trusted_name.struct_version = value;
    return true;
}

/**
 * Handler for tag NOT_VALID_AFTER
 *
 * @param[in] data the tlv data
 * @param[] context the trusted name context
 * @return whether it was successful
 */
static bool handle_not_valid_after(const tlv_data_t *data, s_trusted_name_ctx *context) {
    UNUSED(context);
    const uint8_t app_version[] = {MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION};
    buffer_t version = {0};
    uint16_t version_size = ARRAYLEN(app_version);
    if (!get_buffer_from_tlv_data(data, &version, version_size, version_size)) {
        PRINTF("NOT_VALID_AFTER: failed to extract\n");
        return false;
    }
    for (int i = 0; i < (int) version_size; ++i) {
        if (version.ptr[i] > app_version[i]) {
            break;
        }
        if (version.ptr[i] < app_version[i]) {
            PRINTF("Expired trusted name : %u.%u.%u < %u.%u.%u\n",
                   version.ptr[0],
                   version.ptr[1],
                   version.ptr[2],
                   app_version[0],
                   app_version[1],
                   app_version[2]);
            return false;
        }
    }
    return true;
}

/**
 * Handler for tag CHALLENGE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_challenge(const tlv_data_t *data, s_trusted_name_ctx *context) {
    UNUSED(context);
    return tlv_check_challenge(data);
}

/**
 * Handler for tag SIGNER_KEY_ID
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_signer_key_id(const tlv_data_t *data, s_trusted_name_ctx *context) {
    uint16_t value = 0;
    // For some reason, the key ID is encoded on 2 bytes
    if (!tlv_get_uint16(data, &value, 0, UINT8_MAX)) {
        PRINTF("SIGNER_KEY_ID: error\n");
        return false;
    }
    context->key_id = (e_tn_key_id) value;
    return true;
}

/**
 * Handler for tag SIGNER_ALGO
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_signer_algo(const tlv_data_t *data, s_trusted_name_ctx *context) {
    UNUSED(context);
    uint16_t value = 0;
    // For some reason, the key ID is encoded on 2 bytes
    if (!get_uint16_t_from_tlv_data(data, &value)) {
        PRINTF("SIGNER_ALGO: failed to extract\n");
        return false;
    }
    CHECK_FIELD_VALUE("SIGNER_ALGO", value, SIG_ALGO_SECP256K1);
    return true;
}

/**
 * Handler for tag TRUSTED_NAME
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_trusted_name(const tlv_data_t *data, s_trusted_name_ctx *context) {
    if (!get_string_from_tlv_data(data,
                                  context->trusted_name.name,
                                  1,
                                  sizeof(context->trusted_name.name))) {
        PRINTF("TRUSTED_NAME: failed to extract\n");
        return false;
    }
    return true;
}

/**
 * Handler for tag COIN_TYPE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_coin_type(const tlv_data_t *data, s_trusted_name_ctx *context) {
    UNUSED(context);
    if (!tlv_check_uint8(data, SLIP_44_ETHEREUM)) {
        PRINTF("COIN_TYPE: error\n");
        return false;
    }
    return true;
}

/**
 * Handler for tag ADDRESS
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_address(const tlv_data_t *data, s_trusted_name_ctx *context) {
    return tlv_get_address(data, (uint8_t *) context->trusted_name.addr, false);
}

/**
 * Handler for tag CHAIN_ID
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_chain_id(const tlv_data_t *data, s_trusted_name_ctx *context) {
    return tlv_get_chain_id(data, &context->trusted_name.chain_id);
}

/**
 * Handler for tag NAME_TYPE
 *
 * @param[in] data the tlv data
 * @param[in,out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_trusted_name_type(const tlv_data_t *data, s_trusted_name_ctx *context) {
    uint8_t value = 0;
    if (!get_uint8_t_from_tlv_data(data, &value)) {
        PRINTF("NAME_TYPE: failed to extract\n");
        return false;
    }
    switch (value) {
        case TN_TYPE_ACCOUNT:
        case TN_TYPE_CONTRACT:
        case TN_TYPE_TOKEN:
            break;
        case TN_TYPE_NFT_COLLECTION:
        case TN_TYPE_WALLET:
        case TN_TYPE_CONTEXT_ADDRESS:
        default:
            PRINTF("Error: unsupported trusted name type (%u)!\n", value);
            return false;
    }
    context->trusted_name.name_type = value;
    return true;
}

/**
 * Handler for tag NAME_SOURCE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_trusted_name_source(const tlv_data_t *data, s_trusted_name_ctx *context) {
    uint8_t value = 0;
    if (!get_uint8_t_from_tlv_data(data, &value)) {
        PRINTF("NAME_SOURCE: failed to extract\n");
        return false;
    }
    switch (value) {
        case TN_SOURCE_CAL:
        case TN_SOURCE_ENS:
        case TN_SOURCE_MAB:
            break;
        case TN_SOURCE_LAB:
        case TN_SOURCE_UD:
        case TN_SOURCE_FN:
        case TN_SOURCE_DNS:
        case TN_SOURCE_DYNAMIC_RESOLVER:
        default:
            PRINTF("Error: unsupported trusted name source (%u)!\n", value);
            return false;
    }
    context->trusted_name.name_source = value;
    return true;
}

/**
 * Handler for tag NFT_ID
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_nft_id(const tlv_data_t *data, s_trusted_name_ctx *context) {
    buffer_t field = {0};
    if (!get_buffer_from_tlv_data(data, &field, 1, sizeof(context->trusted_name.nft_id))) {
        PRINTF("NFT_ID: failed to extract\n");
        return false;
    }
    buf_shrink_expand(field.ptr,
                      field.size,
                      context->trusted_name.nft_id,
                      sizeof(context->trusted_name.nft_id));
    return true;
}

/**
 * Handler for tag OWNER
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_owner(const tlv_data_t *data, s_trusted_name_ctx *context) {
    buffer_t field = {0};
    if (!get_buffer_from_tlv_data(data, &field, 1, ADDRESS_LENGTH)) {
        PRINTF("OWNER: failed to extract\n");
        return false;
    }
    buf_shrink_expand(field.ptr, field.size, context->owner, sizeof(context->owner));
    return true;
}

/**
 * Handler for tag SIGNATURE
 *
 * @param[in] data the tlv data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool handle_signature(const tlv_data_t *data, s_trusted_name_ctx *context) {
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

// Define TLV tags and their handlers using X-macro pattern
#define TRUSTED_NAME_TAGS(X)                                                         \
    X(0x01, TAG_STRUCTURE_TYPE, handle_struct_type, ENFORCE_UNIQUE_TAG)              \
    X(0x02, TAG_STRUCTURE_VERSION, handle_struct_version, ENFORCE_UNIQUE_TAG)        \
    X(0x10, TAG_NOT_VALID_AFTER, handle_not_valid_after, ENFORCE_UNIQUE_TAG)         \
    X(0x12, TAG_CHALLENGE, handle_challenge, ENFORCE_UNIQUE_TAG)                     \
    X(0x13, TAG_SIGNER_KEY_ID, handle_signer_key_id, ENFORCE_UNIQUE_TAG)             \
    X(0x14, TAG_SIGNER_ALGO, handle_signer_algo, ENFORCE_UNIQUE_TAG)                 \
    X(0x20, TAG_TRUSTED_NAME, handle_trusted_name, ENFORCE_UNIQUE_TAG)               \
    X(0x21, TAG_COIN_TYPE, handle_coin_type, ENFORCE_UNIQUE_TAG)                     \
    X(0x22, TAG_ADDRESS, handle_address, ENFORCE_UNIQUE_TAG)                         \
    X(0x23, TAG_CHAIN_ID, handle_chain_id, ENFORCE_UNIQUE_TAG)                       \
    X(0x70, TAG_TRUSTED_NAME_TYPE, handle_trusted_name_type, ENFORCE_UNIQUE_TAG)     \
    X(0x71, TAG_TRUSTED_NAME_SOURCE, handle_trusted_name_source, ENFORCE_UNIQUE_TAG) \
    X(0x72, TAG_NFT_ID, handle_nft_id, ENFORCE_UNIQUE_TAG)                           \
    X(0x74, TAG_OWNER, handle_owner, ENFORCE_UNIQUE_TAG)                             \
    X(0x15, TAG_DER_SIGNATURE, handle_signature, ENFORCE_UNIQUE_TAG)

// Forward declaration
static bool trusted_name_common_handler(const tlv_data_t *data, s_trusted_name_ctx *context);

// Generate parser from X-macro
DEFINE_TLV_PARSER(TRUSTED_NAME_TAGS, &trusted_name_common_handler, parse_tlv_trusted_name)

/**
 * Common handler called for all tags to hash them (except signature)
 *
 * @param[in] data the TLV data
 * @param[out] context the trusted name context
 * @return whether it was successful
 */
static bool trusted_name_common_handler(const tlv_data_t *data, s_trusted_name_ctx *context) {
    // Hash everything except signature (tag 0x15)
    if (data->tag != TAG_DER_SIGNATURE) {
        hash_nbytes(data->raw.ptr, data->raw.size, (cx_hash_t *) &context->hash_ctx);
    }
    return true;
}

/**
 * Wrapper function to integrate with existing code
 *
 * @param[in] payload the input buffer containing TLV data
 * @param[out] context the trusted name context
 * @return whether parsing was successful
 */
bool handle_trusted_name_tlv_payload(const buffer_t *payload, s_trusted_name_ctx *context) {
    return parse_tlv_trusted_name(payload, context, &context->received_tags);
}

/**
 * Verify the signature context
 *
 * Verify the SHA-256 hash of the payload against the public key
 *
 * @param[in] context the trusted name context
 * @return whether it was successful
 */
static bool verify_signature(const s_trusted_name_ctx *context) {
    uint8_t hash[INT256_LENGTH];

    if (finalize_hash((cx_hash_t *) &context->hash_ctx, hash, sizeof(hash)) != true) {
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_TRUSTED_NAME,
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
 * @param[in] context Trusted name context
 * @return whether it was successful
 */
static bool verify_fields(const s_trusted_name_ctx *context) {
    // Common required tags for all versions
    if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                 TAG_STRUCTURE_TYPE,
                                 TAG_STRUCTURE_VERSION,
                                 TAG_SIGNER_KEY_ID,
                                 TAG_SIGNER_ALGO,
                                 TAG_DER_SIGNATURE,
                                 TAG_TRUSTED_NAME,
                                 TAG_ADDRESS)) {
        return false;
    }

    switch (context->trusted_name.struct_version) {
        case STRUCT_VERSION_1:
            // Version 1 requires: CHALLENGE and COIN_TYPE
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_CHALLENGE, TAG_COIN_TYPE)) {
                return false;
            }
            break;

        case STRUCT_VERSION_2:
            // Version 2 requires: CHAIN_ID, NAME_TYPE, NAME_SOURCE
            if (!TLV_CHECK_RECEIVED_TAGS(context->received_tags,
                                         TAG_CHAIN_ID,
                                         TAG_TRUSTED_NAME_TYPE,
                                         TAG_TRUSTED_NAME_SOURCE)) {
                return false;
            }
            // Account names require CHALLENGE
            if ((context->trusted_name.name_type == TN_TYPE_ACCOUNT) &&
                (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_CHALLENGE))) {
                PRINTF("Error: trusted account name requires a challenge!\n");
                return false;
            }
            // MAB source requires OWNER
            if ((context->trusted_name.name_source == TN_SOURCE_MAB) &&
                (!TLV_CHECK_RECEIVED_TAGS(context->received_tags, TAG_OWNER))) {
                PRINTF("Error: did not receive an owner for MAB source!\n");
                return false;
            }
            break;
        default:
            PRINTF("Error: unsupported trusted name struct version (%u) !\n",
                   context->trusted_name.struct_version);
            return false;
    }
    return true;
}

/**
 * @brief Print the Trusted name descriptor.
 *
 * @param[in] context Trusted name context
 * Only for debug purpose.
 */
static void print_trusted_name_info(const s_trusted_name_ctx *context) {
    UNUSED(context);
    PRINTF("****************************************************************************\n");
    PRINTF("[TRUSTED NAME] - Registered Trusted Name:\n");
    PRINTF("[TRUSTED NAME] -    Name: %s\n", context->trusted_name.name);
    PRINTF("[TRUSTED NAME] -    Address: %.*h\n", ADDRESS_LENGTH, context->trusted_name.addr);
}

static bool ens_charset(char c) {
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

static bool generic_trusted_name_charset(char c) {
    if (!isalpha((int) c) && !isdigit((int) c)) {
        switch (c) {
            case '.':
            case '-':
            case '_':
            case ' ':
                break;
            default:
                return false;
        }
    }
    return true;
}

/**
 * Check the characters of trusted name with a given check function
 *
 * @param[in] name trusted name
 * @param[in] check_func function to check the character
 */
static bool check_trusted_name(const char *name, bool (*check_func)(char)) {
    if (name == NULL) {
        return false;
    }
    for (int idx = 0; name[idx] != '\0'; ++idx) {
        if (!check_func(name[idx])) {
            PRINTF("Error: unallowed character in trusted name '%c' !\n", name[idx]);
            return false;
        }
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
    s_trusted_name *node = NULL;
    if (!verify_fields(context)) {
        PRINTF("Error: Missing mandatory fields in descriptor!\n");
        return false;
    }

    if (context->trusted_name.struct_version == STRUCT_VERSION_2) {
        switch (context->trusted_name.name_type) {
            case TN_TYPE_ACCOUNT:
                if (context->trusted_name.name_source == TN_SOURCE_CAL) {
                    PRINTF("Error: cannot accept an account name from the CAL!\n");
                    return false;
                }
                break;
            case TN_TYPE_CONTRACT:
            case TN_TYPE_TOKEN:
                if (context->trusted_name.name_source != TN_SOURCE_CAL) {
                    PRINTF("Error: cannot accept a contract name from given source (%u)!\n",
                           context->trusted_name.name_source);
                    return false;
                }
                break;
            default:
                return false;
        }
        // MAB source requires OWNER
        if (context->trusted_name.name_source == TN_SOURCE_MAB) {
            uint8_t wallet_addr[ADDRESS_LENGTH];
            if (get_public_key(wallet_addr, sizeof(wallet_addr)) != SWO_SUCCESS) {
                return false;
            }
            if (memcmp(context->owner, wallet_addr, sizeof(wallet_addr)) != 0) {
                PRINTF("Error: mismatching owner received!\n");
                return false;
            }
        }
    }

    size_t name_length = strnlen(context->trusted_name.name, sizeof(context->trusted_name.name));
    if ((context->trusted_name.struct_version == STRUCT_VERSION_1) ||
        ((context->trusted_name.name_type == TN_TYPE_ACCOUNT) &&
         (context->trusted_name.name_source == TN_SOURCE_ENS))) {
        if ((name_length < 5) ||
            (strncmp(".eth", (char *) &context->trusted_name.name[name_length - 4], 4) != 0)) {
            PRINTF("Unexpected TLD!\n");
            return false;
        }
        if (!check_trusted_name(context->trusted_name.name, &ens_charset)) {
            return false;
        }
    } else {
        if (!check_trusted_name(context->trusted_name.name, &generic_trusted_name_charset)) {
            return false;
        }
    }

    if (!verify_signature(context)) {
        return false;
    }

    if ((node = app_mem_alloc(sizeof(*node))) == NULL) {
        PRINTF("Error: could not allocate trusted name struct!\n");
        return false;
    }
    memcpy(node, &context->trusted_name, sizeof(*node));
    flist_push_back((flist_node_t **) &g_trusted_name_list, (flist_node_t *) node);

    print_trusted_name_info(context);
    return true;
}
