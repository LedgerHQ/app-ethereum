#ifdef HAVE_EIP712_FULL_SUPPORT

#include "filtering.h"
#include "hash_bytes.h"
#include "ethUstream.h"      // INT256_LENGTH
#include "apdu_constants.h"  // APDU return codes
#include "public_keys.h"
#include "context_712.h"
#include "commands_712.h"
#include "typed_data.h"
#include "path.h"
#include "ui_logic.h"

/**
 * Reconstruct the field path and hash it
 *
 * @param[in] hash_ctx the hashing context
 */
static void hash_filtering_path(cx_hash_t *const hash_ctx) {
    const void *field_ptr;
    const char *key;
    uint8_t key_len;

    for (uint8_t i = 0; i < path_get_depth_count(); ++i) {
        if (i > 0) {
            hash_byte('.', hash_ctx);
        }
        if ((field_ptr = path_get_nth_field(i + 1)) != NULL) {
            if ((key = get_struct_field_keyname(field_ptr, &key_len)) != NULL) {
                // field name
                hash_nbytes((uint8_t *) key, key_len, hash_ctx);

                // array levels
                if (struct_field_is_array(field_ptr)) {
                    uint8_t lvl_count;

                    get_struct_field_array_lvls_array(field_ptr, &lvl_count);
                    for (int j = 0; j < lvl_count; ++j) {
                        hash_nbytes((uint8_t *) ".[]", 3, hash_ctx);
                    }
                }
            }
        }
    }
}

/**
 * Verify the provided signature
 *
 * @param[in] dname_length length of provided substitution name
 * @param[in] dname provided substitution name
 * @param[in] sig_length provided signature length
 * @param[in] sig pointer to the provided signature
 * @param[in] type the type of filtering
 * @return whether the signature verification worked or not
 */
static bool verify_filtering_signature(uint8_t dname_length,
                                       const char *const dname,
                                       uint8_t sig_length,
                                       const uint8_t *const sig,
                                       e_filtering_type type) {
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t verifying_key;
    cx_sha256_t hash_ctx;
    uint64_t chain_id;
    cx_err_t error = CX_INTERNAL_ERROR;

    cx_sha256_init(&hash_ctx);

    // Magic number, makes it so a signature of one type can't be used as another
    switch (type) {
        case FILTERING_SHOW_FIELD:
            hash_byte(FILTERING_MAGIC_STRUCT_FIELD, (cx_hash_t *) &hash_ctx);
            break;
        case FILTERING_PROVIDE_MESSAGE_INFO:
            hash_byte(FILTERING_MAGIC_CONTRACT_NAME, (cx_hash_t *) &hash_ctx);
            break;
        default:
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            PRINTF("Invalid filtering type when verifying signature!\n");
            return false;
    }

    // Chain ID
    chain_id = __builtin_bswap64(eip712_context->chain_id);
    hash_nbytes((uint8_t *) &chain_id, sizeof(chain_id), (cx_hash_t *) &hash_ctx);

    // Contract address
    hash_nbytes(eip712_context->contract_addr,
                sizeof(eip712_context->contract_addr),
                (cx_hash_t *) &hash_ctx);

    // Schema hash
    hash_nbytes(eip712_context->schema_hash,
                sizeof(eip712_context->schema_hash),
                (cx_hash_t *) &hash_ctx);

    if (type == FILTERING_SHOW_FIELD) {
        hash_filtering_path((cx_hash_t *) &hash_ctx);
    } else  // FILTERING_PROVIDE_MESSAGE_INFO
    {
        hash_byte(ui_712_remaining_filters(), (cx_hash_t *) &hash_ctx);
    }

    // Display name
    hash_nbytes((uint8_t *) dname, sizeof(char) * dname_length, (cx_hash_t *) &hash_ctx);

    // Finalize hash
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &hash_ctx, CX_LAST, NULL, 0, hash, INT256_LENGTH));

    CX_CHECK(cx_ecfp_init_public_key_no_throw(CX_CURVE_256K1,
                                              LEDGER_SIGNATURE_PUBLIC_KEY,
                                              sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                              &verifying_key));
    if (!cx_ecdsa_verify_no_throw(&verifying_key, hash, sizeof(hash), sig, sig_length)) {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid EIP-712 filtering signature\n");
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
#endif
    }
    return true;
end:
    return false;
}

/**
 * Provide filtering information about upcoming struct field
 *
 * @param[in] payload the raw data received
 * @param[in] length payload length
 * @param[in] type the type of filtering
 * @return if everything went well or not
 */
bool provide_filtering_info(const uint8_t *const payload, uint8_t length, e_filtering_type type) {
    bool ret = false;
    uint8_t dname_len;
    const char *dname;
    uint8_t sig_len;
    const uint8_t *sig;
    uint8_t offset = 0;

    if (type == FILTERING_PROVIDE_MESSAGE_INFO) {
        if (path_get_root_type() != ROOT_DOMAIN) {
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return false;
        }
    } else  // FILTERING_SHOW_FIELD
    {
        if (path_get_root_type() != ROOT_MESSAGE) {
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return false;
        }
    }
    if (length > 0) {
        dname_len = payload[offset++];
        if ((1 + dname_len) < length) {
            dname = (char *) &payload[offset];
            offset += dname_len;
            if (type == FILTERING_PROVIDE_MESSAGE_INFO) {
                ui_712_set_filters_count(payload[offset++]);
            }
            sig_len = payload[offset++];
            sig = &payload[offset];
            offset += sig_len;
            if ((sig_len > 0) && (offset == length)) {
                if ((ret = verify_filtering_signature(dname_len, dname, sig_len, sig, type))) {
                    if (type == FILTERING_PROVIDE_MESSAGE_INFO) {
                        if (!N_storage.verbose_eip712) {
                            ui_712_set_title("Contract", 8);
                            ui_712_set_value(dname, dname_len);
                            ui_712_redraw_generic_step();
                        }
                    } else  // FILTERING_SHOW_FIELD
                    {
                        if (dname_len > 0)  // don't substitute for an empty name
                        {
                            ui_712_set_title(dname, dname_len);
                        }
                        ui_712_flag_field(true, dname_len > 0);
                    }
                }
            }
        }
    }
    return ret;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
