#ifdef HAVE_EIP712_FULL_SUPPORT

#include "filtering.h"
#include "hash_bytes.h"
#include "ethUstream.h" // INT256_LENGTH
#include "apdu_constants.h" // APDU return codes
#include "context.h"
#include "commands_712.h"
#include "typed_data.h"
#include "path.h"
#include "ui_logic.h"


#ifdef HAVE_EIP712_TESTING_KEY
static const uint8_t EIP712_FEEDER_PUBLIC_KEY[] = {
    0x04, 0x4c, 0xca, 0x8f, 0xad, 0x49, 0x6a, 0xa5, 0x04, 0x0a, 0x00, 0xa7, 0xeb, 0x2f,
    0x5c, 0xc3, 0xb8, 0x53, 0x76, 0xd8, 0x8b, 0xa1, 0x47, 0xa7, 0xd7, 0x05, 0x4a, 0x99,
    0xc6, 0x40, 0x56, 0x18, 0x87, 0xfe, 0x17, 0xa0, 0x96, 0xe3, 0x6c, 0x3b, 0x52, 0x3b,
    0x24, 0x4f, 0x3e, 0x2f, 0xf7, 0xf8, 0x40, 0xae, 0x26, 0xc4, 0xe7, 0x7a, 0xd3, 0xbc,
    0x73, 0x9a, 0xf5, 0xde, 0x6f, 0x2d, 0x77, 0xa7, 0xb6
};
#endif // HAVE_EIP712_TESTING_KEY


/**
 * Reconstruct the field path and hash it
 *
 * @param[in] hash_ctx the hashing context
 */
static void hash_filtering_path(cx_hash_t *const hash_ctx)
{
    const void *field_ptr;
    const char *key;
    uint8_t key_len;

    for (uint8_t i = 0; i < path_get_depth_count(); ++i)
    {
        if (i > 0)
        {
            hash_byte('.', hash_ctx);
        }
        if ((field_ptr = path_get_nth_field(i + 1)) != NULL)
        {
            if ((key = get_struct_field_keyname(field_ptr, &key_len)) != NULL)
            {
                // field name
                hash_nbytes((uint8_t*)key, key_len, hash_ctx);

                // array levels
                if (struct_field_is_array(field_ptr))
                {
                    uint8_t lvl_count;

                    get_struct_field_array_lvls_array(field_ptr, &lvl_count);
                    for (int j = 0; j < lvl_count; ++j)
                    {
                        hash_nbytes((uint8_t*)".[]", 3, hash_ctx);
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
                                       e_filtering_type type)
{
    uint8_t hash[INT256_LENGTH];
    cx_ecfp_public_key_t verifying_key;
    cx_sha256_t hash_ctx;
    uint64_t chain_id;

    cx_sha256_init(&hash_ctx);

    // Chain ID
    chain_id = __builtin_bswap64(chainConfig->chainId);
    hash_nbytes((uint8_t*)&chain_id, sizeof(chain_id), (cx_hash_t*)&hash_ctx);

    // Contract address
    hash_nbytes(eip712_context->contract_addr,
                sizeof(eip712_context->contract_addr),
                (cx_hash_t*)&hash_ctx);

    // Schema hash
    hash_nbytes(eip712_context->schema_hash,
                sizeof(eip712_context->schema_hash),
                (cx_hash_t*)&hash_ctx);

    if (type == FILTERING_STRUCT_FIELD)
    {
        hash_filtering_path((cx_hash_t*)&hash_ctx);
    }

    // Display name
    hash_nbytes((uint8_t*)dname,
                sizeof(char) * dname_length,
                (cx_hash_t*)&hash_ctx);

    // Finalize hash
    cx_hash((cx_hash_t*)&hash_ctx,
            CX_LAST,
            NULL,
            0,
            hash,
            INT256_LENGTH);

    cx_ecfp_init_public_key(CX_CURVE_256K1,
#ifdef HAVE_EIP712_TESTING_KEY
                            EIP712_FEEDER_PUBLIC_KEY,
                            sizeof(EIP712_FEEDER_PUBLIC_KEY),
#else
                            LEDGER_SIGNATURE_PUBLIC_KEY,
                            sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
#endif
                            &verifying_key);
    if (!cx_ecdsa_verify(&verifying_key,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         sizeof(hash),
                         sig,
                         sig_length))
    {
#ifndef HAVE_BYPASS_SIGNATURES
        PRINTF("Invalid EIP-712 filtering signature\n");
        return false;
#endif
    }
    return true;
}

/**
 * Provide filtering information about upcoming struct field
 *
 * @param[in] payload the raw data received
 * @param[in] length payload length
 * @param[in] type the type of filtering
 * @return if everything went well or not
 */
bool    provide_filtering_info(const uint8_t *const payload,
                               uint8_t length,
                               e_filtering_type type)
{
    bool ret = false;
    uint8_t dname_len;
    const char *dname;
    uint8_t sig_len;
    const uint8_t *sig;

    if (type == FILTERING_CONTRACT_NAME)
    {
        if (path_get_root_type() != ROOT_DOMAIN)
        {
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return false;
        }
    }
    else // FILTERING_STRUCT_FIELD
    {
        if (path_get_root_type() != ROOT_MESSAGE)
        {
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return false;
        }
    }
    if (length > 0)
    {
        dname_len = payload[0];
        if ((1 + dname_len) < length)
        {
            dname = (char*)&payload[1];
            sig_len = payload[1 + dname_len];
            sig = &payload[1 + dname_len + 1];
            if ((sig_len > 0) && ((1 + dname_len + 1 + sig_len) == length))
            {
                if ((ret = verify_filtering_signature(dname_len, dname, sig_len, sig, type)))
                {
                    if (type == FILTERING_CONTRACT_NAME)
                    {
                        if (!N_storage.verbose_eip712)
                        {
                            ui_712_set_title("Contract", 8);
                            ui_712_set_value(dname, dname_len);
                            ui_712_redraw_generic_step();
                        }
                    }
                    else // FILTERING_STRUCT_FIELD
                    {
                        if (dname_len > 0) // don't substitute for an empty name
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

#endif // HAVE_EIP712_FULL_SUPPORT
