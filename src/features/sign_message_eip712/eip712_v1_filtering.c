#include "eip712_v1_filtering.h"
#include "public_keys.h"
#include "manage_asset_info.h"
#include "eip712_v1_context.h"
#include "typed_data.h"
#include "eip712_v1_ui_logic.h"
#include "os_pki.h"
#include "trusted_name.h"
#include "proxy_info.h"
#include "app_mem_utils.h"
#include "get_public_key.h"
#include "network.h"
#include "format.h"
#include "shared_context.h"
#include "eip712_v1_parse.h"

#define FILT_MAGIC_MESSAGE_INFO      183
#define FILT_MAGIC_CALLDATA_INFO     55
#define FILT_MAGIC_CALLDATA_VALUE    66
#define FILT_MAGIC_CALLDATA_CALLEE   77
#define FILT_MAGIC_CALLDATA_CHAIN_ID 88
#define FILT_MAGIC_CALLDATA_SELECTOR 99
#define FILT_MAGIC_CALLDATA_AMOUNT   110
#define FILT_MAGIC_CALLDATA_SPENDER  121
#define FILT_MAGIC_AMOUNT_JOIN_TOKEN 11
#define FILT_MAGIC_AMOUNT_JOIN_VALUE 22
#define FILT_MAGIC_DATETIME          33
#define FILT_MAGIC_TRUSTED_NAME      44
#define FILT_MAGIC_RAW_FIELD         72

#define TOKEN_IDX_ADDR_IN_DOMAIN 0xff

/**
 * Reconstruct the field path and hash it for the signature and the CRC
 *
 * @param[in] hash_ctx the hashing context
 * @param[in] discarded if the filter targets a field that does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 */
static bool hash_filtering_path(cx_hash_t *hash_ctx, bool discarded, uint32_t *path_crc) {
    const s_struct_712_field *field_ptr;
    const char *key;
    const char *path;
    size_t path_len;

    if (discarded) {
        if ((path = ui_712_get_discarded_path()) == NULL) {
            return false;
        }
        path_len = strlen(path);
        if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) path, path_len) != CX_OK) {
            return false;
        }
        *path_crc = cx_crc32_update(*path_crc, path, path_len);
    } else {
        for (uint8_t i = 0; i < v1_depth_count(); ++i) {
            if (i > 0) {
                if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) ".", 1) != CX_OK) {
                    return false;
                }
                *path_crc = cx_crc32_update(*path_crc, ".", 1);
            }
            if ((field_ptr = v1_nth_field(i + 1)) == NULL) {
                return false;
            }
            if ((key = field_ptr->key_name) != NULL) {
                // field name
                if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) key, strlen(key)) != CX_OK) {
                    return false;
                }
                *path_crc = cx_crc32_update(*path_crc, key, strlen(key));

                // array levels
                if (field_ptr->array_level_count > 0) {
                    for (int j = 0; j < field_ptr->array_level_count; ++j) {
                        if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) ".[]", 3) != CX_OK) {
                            return false;
                        }
                        *path_crc = cx_crc32_update(*path_crc, ".[]", 3);
                    }
                }
            }
        }
    }
    // so it is only usable for the following filter
    ui_712_clear_discarded_path();
    return true;
}

/**
 * Begin the hashing for signature verification
 *
 * @param[in] hash_ctx hashing context
 * @param[in] magic magic number used in the signature
 * @return \ref true
 */
static bool sig_verif_start(cx_sha256_t *hash_ctx, uint8_t magic) {
    uint64_t chain_id;
    const uint8_t *addr;

    if (cx_sha256_init_no_throw(hash_ctx) != CX_OK) {
        return false;
    }

    // Magic number, makes it so a signature of one type can't be used as another
    if (cx_hash_update((cx_hash_t *) hash_ctx, &magic, sizeof(magic)) != CX_OK) {
        return false;
    }

    // Chain ID
    uint64_t domain_chain_id;
    // optional in EIP-712 domain; if absent, 0 is used in the signature hash
    if (!td_get_domain_chain_id(&domain_chain_id)) {
        domain_chain_id = 0;
    }
    chain_id = __builtin_bswap64(domain_chain_id);
    if (cx_hash_update((cx_hash_t *) hash_ctx, (uint8_t *) &chain_id, sizeof(chain_id)) != CX_OK) {
        return false;
    }

    // Contract address
    // we can't compare the returned address with anything since filtering payloads are signed on an
    // address which is not provided
    uint8_t domain_contract[ADDRESS_LENGTH];
    // optional in EIP-712 domain; if absent, 0 is used in the signature hash
    if (!td_get_domain_contract_addr(domain_contract)) {
        explicit_bzero(domain_contract, sizeof(domain_contract));
    }
    if ((addr = get_implem_contract(&domain_chain_id, domain_contract, NULL)) == NULL) {
        addr = domain_contract;
    }
    if (cx_hash_update((cx_hash_t *) hash_ctx, addr, ADDRESS_LENGTH) != CX_OK) {
        return false;
    }

    // Schema hash
    return cx_hash_update((cx_hash_t *) hash_ctx,
                          eip712_v1_context->schema_hash,
                          sizeof(eip712_v1_context->schema_hash)) == CX_OK;
}

/**
 * End the hashing & do the signature verification
 *
 * @param[in] hash_ctx hashing context
 * @param[in] sig signature
 * @param[in] sig_length signature length
 * @return whether the signature verification worked or not
 */
static bool sig_verif_end(cx_sha256_t *hash_ctx, const uint8_t *sig, uint8_t sig_length) {
    uint8_t hash[INT256_LENGTH];

    if (cx_hash_final((cx_hash_t *) hash_ctx, hash) != CX_OK) {
        return false;
    }

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    LEDGER_SIGNATURE_PUBLIC_KEY,
                                    sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
                                    (uint8_t *) sig,
                                    sig_length) != true) {
        return false;
    }

    return true;
}

/**
 * Check if the current element's typename matches the expected one
 *
 * @param[in] expected the typename we expect
 * @return whether it is a match or not
 */
static bool check_typename(const char *expected) {
    uint8_t typename_len = 0;
    const char *typename;

    if ((typename = td_get_struct_field_typename(v1_get_current_field())) == NULL) {
        return false;
    }
    typename_len = strlen(typename);
    if ((typename_len != strlen(expected)) || (strncmp(typename, expected, typename_len) != 0)) {
        PRINTF("Error: expected field of type \"%s\" but got \"", expected);
        for (int i = 0; i < typename_len; ++i) PRINTF("%c", typename[i]);
        PRINTF("\" instead.\n");
        return false;
    }
    return true;
}

/**
 * Command to give the message information
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @return whether it was successful or not
 */
bool filtering_message_info(const uint8_t *payload, uint8_t length) {
    uint8_t name_len;
    const char *name;
    uint8_t filters_count;
    uint8_t sig_len;
    const uint8_t *sig;
    uint8_t offset = 0;

    // The trusted signature binds the displayed name to the domain chain ID and
    // contract, so they must be final: require the domain value tree to be fully
    // built and the message root not started yet.
    if (!v1_domain_complete()) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(name_len)) > length) {
        return false;
    }
    name_len = payload[offset++];
    if ((offset + name_len) > length) {
        return false;
    }
    name = (char *) &payload[offset];
    offset += name_len;
    if ((offset + sizeof(filters_count)) > length) {
        return false;
    }
    filters_count = payload[offset++];
    if (filters_count > MAX_FILTERS) {
        PRINTF("%u filters planned but can only store up to %u.\n", filters_count, MAX_FILTERS);
        return false;
    }
    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_MESSAGE_INFO)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &filters_count, sizeof(filters_count)) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) name, name_len) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    ui_712_set_filters_count(filters_count);
    if (!N_storage.verbose_eip712) {
        uint64_t domain_chain_id;

        if (!ui_712_set_title("Contract", 8)) {
            return false;
        }
        if (!ui_712_set_value(name, name_len)) {
            return false;
        }
        if (td_get_domain_chain_id(&domain_chain_id)) {
            // Adds the Network pair after Contract while root is still ROOT_DOMAIN; skipped if
            // chain matches the app's own
            if (ui_712_get_filtering_mode() == EIP712_FILTERING_FULL &&
                domain_chain_id != g_chain_config->chain_id) {
                const char *network_name = get_network_name_from_chain_id(&domain_chain_id);
                if (!ui_712_set_title("Network", 7)) {
                    return false;
                }
                if (network_name != NULL) {
                    if (!ui_712_set_value(network_name, strlen(network_name))) {
                        return false;
                    }
                } else {
                    if (!format_u64(strings.tmp.tmp, NETWORK_STRING_MAX_SIZE, domain_chain_id)) {
                        return false;
                    }
                    if (!ui_712_set_value(strings.tmp.tmp, strlen(strings.tmp.tmp))) {
                        return false;
                    }
                }
            }
        }
        return ui_712_continue_or_finish();
    }
    return true;
}

/**
 * Check if given path matches the backed-up path
 *
 * A match is found as long as the given path starts with the backed-up path.
 *
 * @param[in] path given path
 * @param[in] path_len length of the path
 * @param[out] offset_ptr offset to where the comparison stopped
 * @return whether a match was found or not
 */
static bool matches_backup_path(const char *path, uint8_t path_len, uint8_t *offset_ptr) {
    const s_struct_712_field *field_ptr;
    const char *key;
    uint8_t offset = 0;

    for (uint8_t i = 0; i < v1_backup_depth_count(); ++i) {
        if (i > 0) {
            if (((offset + 1) > path_len) || (memcmp(path + offset, ".", 1) != 0)) {
                return false;
            }
            offset += 1;
        }
        if ((field_ptr = v1_backup_nth_field(i + 1)) != NULL) {
            if ((key = field_ptr->key_name) != NULL) {
                // field name
                if (((offset + strlen(key)) > path_len) ||
                    (memcmp(path + offset, key, strlen(key)) != 0)) {
                    return false;
                }
                offset += strlen(key);

                // array levels
                if (field_ptr->array_level_count > 0) {
                    for (int j = 0; j < field_ptr->array_level_count; ++j) {
                        if (((offset + 3) > path_len) || (memcmp(path + offset, ".[]", 3) != 0)) {
                            return false;
                        }
                        offset += 3;
                    }
                }
            }
        }
    }
    if (offset_ptr != NULL) {
        *offset_ptr = offset;
    }
    return true;
}

/**
 * Command to provide the filter path of a discarded filtered field
 *
 * Some filtered fields are discarded/never received because they are contained in an array
 * that turns out to be empty.
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @return whether it was successful or not
 */
bool filtering_discarded_path(const uint8_t *payload, uint8_t length) {
    uint8_t path_len;
    const char *path;
    uint8_t offset = 0;
    uint8_t path_offset;

    if ((offset + sizeof(path_len)) > length) {
        return false;
    }
    path_len = payload[offset++];
    if ((offset + path_len) > length) {
        return false;
    }
    path = (char *) &payload[offset];
    if (!matches_backup_path(path, path_len, &path_offset)) {
        return false;
    }
    if (!v1_backup_exists(path + path_offset, path_len - path_offset)) {
        return false;
    }
    ui_712_set_discarded_path(path, path_len);
    return true;
}

/**
 * Command to process the upcoming field as a nested TX spender
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_calldata_spender(const uint8_t *payload,
                                uint8_t length,
                                bool discarded,
                                uint32_t *path_crc) {
    uint8_t offset = 0;
    uint8_t index;
    uint8_t sig_len;
    const uint8_t *sig;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(index)) > length) {
        return false;
    }
    index = payload[offset++];

    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_CALLDATA_SPENDER)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &index, sizeof(index)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (get_calldata_info(index) == NULL) {
        PRINTF("Error: no matching calldata info (index=%u)\n", index);
        return false;
    }
    ui_712_flag_field(false, false, false, false, false, true);
    calldata_info_set_state(index, EIP712_CALLDATA_SPENDER);
    return true;
}

/**
 * Command to process the upcoming field as a nested TX amount
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_calldata_amount(const uint8_t *payload,
                               uint8_t length,
                               bool discarded,
                               uint32_t *path_crc) {
    uint8_t offset = 0;
    uint8_t index;
    uint8_t sig_len;
    const uint8_t *sig;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(index)) > length) {
        return false;
    }
    index = payload[offset++];

    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_CALLDATA_AMOUNT)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &index, sizeof(index)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (get_calldata_info(index) == NULL) {
        PRINTF("Error: no matching calldata info (index=%u)\n", index);
        return false;
    }
    ui_712_flag_field(false, false, false, false, false, true);
    calldata_info_set_state(index, EIP712_CALLDATA_AMOUNT);
    return true;
}

/**
 * Command to process the upcoming field as a nested TX calldata selector
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_calldata_selector(const uint8_t *payload,
                                 uint8_t length,
                                 bool discarded,
                                 uint32_t *path_crc) {
    uint8_t offset = 0;
    uint8_t index;
    uint8_t sig_len;
    const uint8_t *sig;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(index)) > length) {
        return false;
    }
    index = payload[offset++];

    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_CALLDATA_SELECTOR)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &index, sizeof(index)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (get_calldata_info(index) == NULL) {
        PRINTF("Error: no matching calldata info (index=%u)\n", index);
        return false;
    }
    ui_712_flag_field(false, false, false, false, false, true);
    calldata_info_set_state(index, EIP712_CALLDATA_SELECTOR);
    return true;
}

/**
 * Command to process the upcoming field as a nested TX chain ID
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_calldata_chain_id(const uint8_t *payload,
                                 uint8_t length,
                                 bool discarded,
                                 uint32_t *path_crc) {
    uint8_t offset = 0;
    uint8_t index;
    uint8_t sig_len;
    const uint8_t *sig;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(index)) > length) {
        return false;
    }
    index = payload[offset++];

    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_CALLDATA_CHAIN_ID)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &index, sizeof(index)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (get_calldata_info(index) == NULL) {
        PRINTF("Error: no matching calldata info (index=%u)\n", index);
        return false;
    }
    ui_712_flag_field(false, false, false, false, false, true);
    calldata_info_set_state(index, EIP712_CALLDATA_CHAIN_ID);
    return true;
}

/**
 * Command to process the upcoming field as a nested TX callee
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_calldata_callee(const uint8_t *payload,
                               uint8_t length,
                               bool discarded,
                               uint32_t *path_crc) {
    uint8_t offset = 0;
    uint8_t index;
    uint8_t sig_len;
    const uint8_t *sig;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(index)) > length) {
        return false;
    }
    index = payload[offset++];

    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_CALLDATA_CALLEE)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &index, sizeof(index)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (get_calldata_info(index) == NULL) {
        PRINTF("Error: no matching calldata info (index=%u)\n", index);
        return false;
    }
    ui_712_flag_field(false, false, false, false, false, true);
    calldata_info_set_state(index, EIP712_CALLDATA_CALLEE);
    return true;
}

/**
 * Command to process the upcoming field as a nested TX calldata
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_calldata_value(const uint8_t *payload,
                              uint8_t length,
                              bool discarded,
                              uint32_t *path_crc) {
    uint8_t offset = 0;
    uint8_t index;
    uint8_t sig_len;
    const uint8_t *sig;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(index)) > length) {
        return false;
    }
    index = payload[offset++];

    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_CALLDATA_VALUE)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &index, sizeof(index)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (get_calldata_info(index) == NULL) {
        PRINTF("Error: no matching calldata info (index=%u)\n", index);
        return false;
    }
    ui_712_flag_field(false, false, false, false, false, true);
    calldata_info_set_state(index, EIP712_CALLDATA_VALUE);
    return true;
}

/**
 * Command to give the calldata info/context (not attached to a field)
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @return whether it was successful or not
 */
bool filtering_calldata_info(const uint8_t *payload, uint8_t length) {
    uint8_t offset = 0;
    uint8_t index;
    bool value_flag;
    e_calldata_addr_flag callee_flag;
    bool chain_id_flag;
    bool selector_flag;
    bool amount_flag;
    e_calldata_addr_flag spender_flag;
    uint8_t sig_len;
    const uint8_t *sig;
    s_eip712_calldata_info *calldata_info;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(index)) > length) {
        return false;
    }
    index = payload[offset++];

    if ((offset + sizeof(value_flag)) > length) {
        return false;
    }
    value_flag = payload[offset++];
    // mandatory
    if (!value_flag) return false;

    if ((offset + sizeof(callee_flag)) > length) {
        return false;
    }
    callee_flag = payload[offset++];
    switch (callee_flag) {
        case CALLDATA_FLAG_ADDR_FILTER:
        case CALLDATA_FLAG_ADDR_VERIFYING_CONTRACT:
            break;
        default:
            return false;
    }

    if ((offset + sizeof(chain_id_flag)) > length) {
        return false;
    }
    chain_id_flag = payload[offset++];

    if ((offset + sizeof(selector_flag)) > length) {
        return false;
    }
    selector_flag = payload[offset++];

    if ((offset + sizeof(amount_flag)) > length) {
        return false;
    }
    amount_flag = payload[offset++];

    if ((offset + sizeof(spender_flag)) > length) {
        return false;
    }
    spender_flag = payload[offset++];
    switch (spender_flag) {
        case CALLDATA_FLAG_ADDR_NONE:
        case CALLDATA_FLAG_ADDR_FILTER:
        case CALLDATA_FLAG_ADDR_VERIFYING_CONTRACT:
            break;
        default:
            return false;
    }

    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_CALLDATA_INFO)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &index, sizeof(index)) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) &value_flag, sizeof(value_flag)) !=
        CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) &callee_flag, sizeof(callee_flag)) !=
        CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx,
                       (uint8_t *) &chain_id_flag,
                       sizeof(chain_id_flag)) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx,
                       (uint8_t *) &selector_flag,
                       sizeof(selector_flag)) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) &amount_flag, sizeof(amount_flag)) !=
        CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) &spender_flag, sizeof(spender_flag)) !=
        CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }
    if (APP_MEM_CALLOC((void **) &calldata_info, sizeof(*calldata_info)) == false) {
        return false;
    }

    calldata_info->index = index;

    calldata_info->value_state = CALLDATA_INFO_PARAM_UNSET;
    switch (callee_flag) {
        case CALLDATA_FLAG_ADDR_FILTER:
            calldata_info->callee_state = CALLDATA_INFO_PARAM_UNSET;
            break;
        case CALLDATA_FLAG_ADDR_VERIFYING_CONTRACT:
            if (!td_get_domain_contract_addr(calldata_info->callee)) {
                return false;
            }
            calldata_info->callee_state = CALLDATA_INFO_PARAM_SET;
            break;
        default:
            break;
    }
    if (chain_id_flag) {
        calldata_info->chain_id_state = CALLDATA_INFO_PARAM_UNSET;
    } else {
        if (!td_get_domain_chain_id(&calldata_info->chain_id)) {
            return false;
        }
        calldata_info->chain_id_state = CALLDATA_INFO_PARAM_SET;
    }
    if (selector_flag) calldata_info->selector_state = CALLDATA_INFO_PARAM_UNSET;
    if (amount_flag) calldata_info->amount_state = CALLDATA_INFO_PARAM_UNSET;
    switch (spender_flag) {
        case CALLDATA_FLAG_ADDR_VERIFYING_CONTRACT:
            if (!td_get_domain_contract_addr(calldata_info->spender)) {
                return false;
            }
            calldata_info->spender_state = CALLDATA_INFO_PARAM_SET;
            break;
        case CALLDATA_FLAG_ADDR_NONE:
            get_public_key(calldata_info->spender, sizeof(calldata_info->spender));
            calldata_info->spender_state = CALLDATA_INFO_PARAM_SET;
            break;
        case CALLDATA_FLAG_ADDR_FILTER:
            calldata_info->spender_state = CALLDATA_INFO_PARAM_UNSET;
            break;
        default:
            break;
    }
    add_calldata_info(calldata_info);
    PRINTF("New calldata info (index=%u)\n", index);
    return true;
}

/**
 * Command to display a field as a trusted name
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_trusted_name(const uint8_t *payload,
                            uint8_t length,
                            bool discarded,
                            uint32_t *path_crc) {
    uint8_t name_len;
    const char *name;
    uint8_t type_count;
    e_name_type *types;
    uint8_t source_count;
    e_name_source *sources;
    uint8_t sig_len;
    const uint8_t *sig;
    uint8_t offset = 0;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(name_len)) > length) {
        return false;
    }
    name_len = payload[offset++];
    if ((offset + name_len) > length) {
        return false;
    }
    name = (char *) &payload[offset];
    offset += name_len;
    if ((offset + sizeof(type_count)) > length) {
        return false;
    }
    type_count = payload[offset++];
    if (type_count > TN_TYPE_COUNT) {
        return false;
    }
    if ((offset + type_count) > length) {
        return false;
    }
    types = (e_name_type *) &payload[offset];
    for (int i = 0; i < type_count; ++i) {
        switch (types[i]) {
            case TN_TYPE_ACCOUNT:
            case TN_TYPE_CONTRACT:
            case TN_TYPE_NFT_COLLECTION:
            case TN_TYPE_TOKEN:
            case TN_TYPE_WALLET:
            case TN_TYPE_CONTEXT_ADDRESS:
                break;
            default:
                return false;
        }
    }
    offset += type_count;
    if ((offset + sizeof(source_count)) > length) {
        return false;
    }
    source_count = payload[offset++];
    if (source_count > TN_SOURCE_COUNT) {
        return false;
    }
    if ((offset + source_count) > length) {
        return false;
    }
    sources = (e_name_source *) &payload[offset];
    for (int i = 0; i < source_count; ++i) {
        switch (sources[i]) {
            case TN_SOURCE_LAB:
            case TN_SOURCE_CAL:
            case TN_SOURCE_ENS:
            case TN_SOURCE_UD:
            case TN_SOURCE_FN:
            case TN_SOURCE_DNS:
            case TN_SOURCE_DYNAMIC_RESOLVER:
            case TN_SOURCE_MAB:
                break;
            default:
                return false;
        }
    }
    offset += source_count;
    //
    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_TRUSTED_NAME)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) name, name_len) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) types, type_count) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) sources, source_count) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (!check_typename("address")) {
        return false;
    }
    if (name_len > 0) {  // don't substitute for an empty name
        if (!ui_712_set_title(name, name_len)) {
            return false;
        }
    }
    ui_712_flag_field(true, name_len > 0, false, false, true, false);
    ui_712_set_trusted_name_requirements(type_count, types, source_count, sources);
    return true;
}
/**
 * Command to display a field as a date-time
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_date_time(const uint8_t *payload,
                         uint8_t length,
                         bool discarded,
                         uint32_t *path_crc) {
    uint8_t name_len;
    const char *name;
    uint8_t sig_len;
    const uint8_t *sig;
    uint8_t offset = 0;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(name_len)) > length) {
        return false;
    }
    name_len = payload[offset++];
    if ((offset + name_len) > length) {
        return false;
    }
    name = (char *) &payload[offset];
    offset += name_len;
    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_DATETIME)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) name, name_len) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (!check_typename("uint")) {
        return false;
    }
    if (name_len > 0) {  // don't substitute for an empty name
        if (!ui_712_set_title(name, name_len)) {
            return false;
        }
    }
    ui_712_flag_field(true, name_len > 0, false, true, false, false);
    return true;
}

/**
 * Command to display a field as an amount-join (token part)
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_amount_join_token(const uint8_t *payload,
                                 uint8_t length,
                                 bool discarded,
                                 uint32_t *path_crc) {
    uint8_t join_id;
    uint8_t sig_len;
    const uint8_t *sig;
    uint8_t offset = 0;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(join_id)) > length) {
        return false;
    }
    join_id = payload[offset++];
    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_AMOUNT_JOIN_TOKEN)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &join_id, sizeof(join_id)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (!check_typename("address")) {
        return false;
    }
    ui_712_flag_field(false, false, true, false, false, false);
    ui_712_token_join_prepare_addr_check(join_id);
    return true;
}

/**
 * Command to display a field as an amount-join (value part)
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_amount_join_value(const uint8_t *payload,
                                 uint8_t length,
                                 bool discarded,
                                 uint32_t *path_crc) {
    uint8_t name_len;
    const char *name;
    uint8_t join_id;
    uint8_t sig_len;
    const uint8_t *sig;
    uint8_t offset = 0;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(name_len)) > length) {
        return false;
    }
    name_len = payload[offset++];
    if ((offset + name_len) > length) {
        return false;
    }
    if (name_len == 0) {
        return false;
    }
    name = (char *) &payload[offset];
    offset += name_len;
    if ((offset + sizeof(join_id)) > length) {
        return false;
    }
    join_id = payload[offset++];
    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_AMOUNT_JOIN_VALUE)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) name, name_len) != CX_OK) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, &join_id, sizeof(join_id)) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    // Handling
    if (discarded) return true;
    if (join_id == TOKEN_IDX_ADDR_IN_DOMAIN) {
        // Permit (ERC-2612)
        ui_712_token_join_prepare_addr_check(join_id);
        // simulate as if we had received a token-join addr
        uint8_t domain_contract[ADDRESS_LENGTH];
        if (!td_get_domain_contract_addr(domain_contract)) {
            return false;
        }
        if (!ui_712_set_amount_join_token_addr(domain_contract)) {
            return false;
        }
    }
    if (!check_typename("uint")) {
        return false;
    }
    ui_712_flag_field(false, false, true, false, false, false);
    return ui_712_token_join_prepare_amount(join_id, name, name_len);
}

/**
 * Command to display a field raw (without formatting)
 *
 * @param[in] payload the payload to parse
 * @param[in] length the payload length
 * @param[in] discarded if the filter targets a field that is does not exist (within an empty array)
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_raw_field(const uint8_t *payload,
                         uint8_t length,
                         bool discarded,
                         uint32_t *path_crc) {
    uint8_t name_len;
    const char *name;
    uint8_t sig_len;
    const uint8_t *sig;
    uint8_t offset = 0;

    if (v1_get_root_type() != ROOT_MESSAGE) {
        return false;
    }

    // Parsing
    if ((offset + sizeof(name_len)) > length) {
        return false;
    }
    name_len = payload[offset++];
    if ((offset + name_len) > length) {
        return false;
    }
    name = (char *) &payload[offset];
    offset += name_len;
    if ((offset + sizeof(sig_len)) > length) {
        return false;
    }
    sig_len = payload[offset++];
    if ((offset + sig_len) != length) {
        return false;
    }
    sig = &payload[offset];

    // Verification
    cx_sha256_t hash_ctx;
    if (!sig_verif_start(&hash_ctx, FILT_MAGIC_RAW_FIELD)) {
        return false;
    }
    if (!hash_filtering_path((cx_hash_t *) &hash_ctx, discarded, path_crc)) {
        return false;
    }
    if (cx_hash_update((cx_hash_t *) &hash_ctx, (uint8_t *) name, name_len) != CX_OK) {
        return false;
    }
    if (!sig_verif_end(&hash_ctx, sig, sig_len)) {
        return false;
    }

    if (!discarded) {
        // Handling
        if (name_len > 0) {  // don't substitute for an empty name
            if (!ui_712_set_title(name, name_len)) {
                return false;
            }
        }
        ui_712_flag_field(true, name_len > 0, false, false, false, false);
    }
    return true;
}

/**
 * Compute the CRC of the path the current field is at.
 *
 * @param[out] path_crc pointer to the CRC of the filter path
 * @return whether it was successful or not
 */
bool filtering_compute_current_path_crc(uint32_t *path_crc) {
    cx_sha256_t hash_ctx;

    if (!sig_verif_start(&hash_ctx, 0)) {
        return false;
    }
    // reuse the path hashing; the magic byte does not matter for a CRC
    return hash_filtering_path((cx_hash_t *) &hash_ctx, false, path_crc);
}
