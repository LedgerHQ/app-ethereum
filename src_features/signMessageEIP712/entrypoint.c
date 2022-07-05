#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "apdu_constants.h"
#include "eip712.h"
#include "mem.h"
#include "type_hash.h"
#include "context.h"
#include "sol_typenames.h"
#include "field_hash.h"
#include "path.h"
#include "shared_context.h"
#include "ui_logic.h"
#include "common_712.h"
#include "path.h"
#include "typed_data.h"

#ifdef HAVE_EIP712_TESTING_KEY
static const uint8_t EIP712_FEEDER_PUBLIC_KEY[] = {
    0x04, 0x4c, 0xca, 0x8f, 0xad, 0x49, 0x6a, 0xa5, 0x04, 0x0a, 0x00, 0xa7, 0xeb, 0x2f,
    0x5c, 0xc3, 0xb8, 0x53, 0x76, 0xd8, 0x8b, 0xa1, 0x47, 0xa7, 0xd7, 0x05, 0x4a, 0x99,
    0xc6, 0x40, 0x56, 0x18, 0x87, 0xfe, 0x17, 0xa0, 0x96, 0xe3, 0x6c, 0x3b, 0x52, 0x3b,
    0x24, 0x4f, 0x3e, 0x2f, 0xf7, 0xf8, 0x40, 0xae, 0x26, 0xc4, 0xe7, 0x7a, 0xd3, 0xbc,
    0x73, 0x9a, 0xf5, 0xde, 0x6f, 0x2d, 0x77, 0xa7, 0xb6
};
#endif // HAVE_EIP712_TESTING_KEY

void    handle_eip712_return_code(bool ret)
{
    if (ret)
    {
        apdu_response_code = APDU_RESPONSE_OK;
    }
    *(uint16_t*)G_io_apdu_buffer = __builtin_bswap16(apdu_response_code);

    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);

    if (!ret)
    {
        eip712_context_deinit();
    }
}

bool    handle_eip712_struct_def(const uint8_t *const apdu_buf)
{
    bool ret = true;

    if (eip712_context == NULL)
    {
        ret = eip712_context_init();
    }
    if (ret)
    {
        switch (apdu_buf[OFFSET_P2])
        {
            case P2_NAME:
                ret = set_struct_name(apdu_buf[OFFSET_LC], &apdu_buf[OFFSET_CDATA]);
                break;
            case P2_FIELD:
                ret = set_struct_field(apdu_buf);
                break;
            default:
                PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                        apdu_buf[OFFSET_P2],
                        apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
                ret = false;
        }
    }
    handle_eip712_return_code(ret);
    return ret;
}

bool    handle_eip712_struct_impl(const uint8_t *const apdu_buf)
{
    bool ret = false;
    bool reply_apdu = true;

    if (eip712_context == NULL)
    {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    else
    {
        switch (apdu_buf[OFFSET_P2])
        {
            case P2_NAME:
                // set root type
                if ((ret = path_set_root((char*)&apdu_buf[OFFSET_CDATA],
                                         apdu_buf[OFFSET_LC])))
                {
                    if (N_storage.verbose_eip712)
                    {
                        ui_712_review_struct(path_get_root());
                        reply_apdu = false;
                    }
                    ui_712_field_flags_reset();
                }
                break;
            case P2_FIELD:
                if ((ret = field_hash(&apdu_buf[OFFSET_CDATA],
                                      apdu_buf[OFFSET_LC],
                                      apdu_buf[OFFSET_P1] != P1_COMPLETE)))
                {
                    reply_apdu = false;
                }
                break;
            case P2_ARRAY:
                ret = path_new_array_depth(apdu_buf[OFFSET_CDATA]);
                break;
            default:
                PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                       apdu_buf[OFFSET_P2],
                       apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
        }
    }
    if (reply_apdu)
    {
        handle_eip712_return_code(ret);
    }
    return ret;
}

#include "hash_bytes.h"
static bool verify_filtering_signature(uint8_t dname_length,
                                       const char *const dname,
                                       uint8_t sig_length,
                                       const uint8_t *const sig,
                                       uint8_t p1)
{
    const void *field_ptr;
    const char *key;
    uint8_t key_len;
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

    if (p1 == P1_FIELD_NAME)
    {
        uint8_t depth_count = path_get_depth_count();

        for (uint8_t i = 0; i < depth_count; ++i)
        {
            if (i > 0)
            {
                hash_byte('.', (cx_hash_t*)&hash_ctx);
            }
            if ((field_ptr = path_get_nth_field(i + 1)) != NULL)
            {
                if ((key = get_struct_field_keyname(field_ptr, &key_len)) != NULL)
                {
                    // field name
                    hash_nbytes((uint8_t*)key, key_len, (cx_hash_t*)&hash_ctx);

                    // array levels
                    if (struct_field_is_array(field_ptr))
                    {
                        uint8_t lvl_count;

                        get_struct_field_array_lvls_array(field_ptr, &lvl_count);
                        for (int j = 0; j < lvl_count; ++j)
                        {
                            hash_nbytes((uint8_t*)".[]", 3, (cx_hash_t*)&hash_ctx);
                        }
                    }
                }
            }
        }
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

bool    provide_filtering_info(const uint8_t *const payload, uint8_t length, uint8_t p1)
{
    bool ret = false;
    uint8_t dname_len;
    const char *dname;
    uint8_t sig_len;
    const uint8_t *sig;

    if (p1 == P1_CONTRACT_NAME)
    {
        if (path_get_root_type() != ROOT_DOMAIN)
        {
            apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
            return false;
        }
    }
    else // P1_FIELD_NAME
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
                if ((ret = verify_filtering_signature(dname_len, dname, sig_len, sig, p1)))
                {
                    if (p1 == P1_CONTRACT_NAME)
                    {
                        if (!N_storage.verbose_eip712)
                        {
                            ui_712_set_title("Contract", 8);
                            ui_712_set_value(dname, dname_len);
                            ui_712_redraw_generic_step();
                        }
                    }
                    else // P1_FIELD_NAME
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

#include "hash_bytes.h"
#include "format_hash_field_type.h"

bool    compute_schema_hash(void)
{
    const void *struct_ptr;
    uint8_t structs_count;
    const void *field_ptr;
    uint8_t fields_count;
    const char *name;
    uint8_t name_length;
    cx_sha256_t hash_ctx; // sha224

    cx_sha224_init(&hash_ctx);

    struct_ptr = get_structs_array(eip712_context->structs_array, &structs_count);
    hash_byte('{', (cx_hash_t*)&hash_ctx);
    while (structs_count-- > 0)
    {
        name = get_struct_name(struct_ptr, &name_length);
        hash_byte('"', (cx_hash_t*)&hash_ctx);
        hash_nbytes((uint8_t*)name, name_length, (cx_hash_t*)&hash_ctx);
        hash_nbytes((uint8_t*)"\":[", 3, (cx_hash_t*)&hash_ctx);
        field_ptr = get_struct_fields_array(struct_ptr, &fields_count);
        while (fields_count-- > 0)
        {
            hash_nbytes((uint8_t*)"{\"name\":\"", 9, (cx_hash_t*)&hash_ctx);
            name = get_struct_field_keyname(field_ptr, &name_length);
            hash_nbytes((uint8_t*)name, name_length, (cx_hash_t*)&hash_ctx);
            hash_nbytes((uint8_t*)"\",\"type\":\"", 10, (cx_hash_t*)&hash_ctx);
            if (!format_hash_field_type(field_ptr, (cx_hash_t*)&hash_ctx))
            {
                return false;
            }
            hash_nbytes((uint8_t*)"\"}", 2, (cx_hash_t*)&hash_ctx);
            if (fields_count > 0)
            {
                hash_byte(',', (cx_hash_t*)&hash_ctx);
            }
            field_ptr = get_next_struct_field(field_ptr);
        }
        hash_byte(']', (cx_hash_t*)&hash_ctx);
        if (structs_count > 0)
        {
            hash_byte(',', (cx_hash_t*)&hash_ctx);
        }
        struct_ptr = get_next_struct(struct_ptr);
    }
    hash_byte('}', (cx_hash_t*)&hash_ctx);

    // copy hash into context struct
    cx_hash((cx_hash_t*)&hash_ctx,
            CX_LAST,
            NULL,
            0,
            eip712_context->schema_hash,
            sizeof(eip712_context->schema_hash));
    return true;
}

bool    handle_eip712_filtering(const uint8_t *const apdu_buf)
{
    bool ret = true;
    bool reply_apdu = true;

    if (eip712_context == NULL)
    {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        ret = false;
    }
    else
    {
        switch (apdu_buf[OFFSET_P1])
        {
            case P1_ACTIVATE:
                if (!N_storage.verbose_eip712)
                {
                    ui_712_set_filtering_mode(EIP712_FILTERING_FULL);
                    ret = compute_schema_hash();
                }
                break;
            case P1_CONTRACT_NAME:
            case P1_FIELD_NAME:
                if (ui_712_get_filtering_mode() == EIP712_FILTERING_FULL)
                {
                    ret = provide_filtering_info(&apdu_buf[OFFSET_CDATA],
                            apdu_buf[OFFSET_LC],
                            apdu_buf[OFFSET_P1]);
                    if ((apdu_buf[OFFSET_P1] == P1_CONTRACT_NAME) && ret)
                    {
                        reply_apdu = false;
                    }
                }
                break;
            default:
                PRINTF("Unknown P1 0x%x for APDU 0x%x\n",
                        apdu_buf[OFFSET_P1],
                        apdu_buf[OFFSET_INS]);
                apdu_response_code = APDU_RESPONSE_INVALID_P1_P2;
                ret = false;
        }
    }
    if (reply_apdu)
    {
        handle_eip712_return_code(ret);
    }
    return ret;
}

bool    handle_eip712_sign(const uint8_t *const apdu_buf)
{
    bool ret = false;

    if (eip712_context == NULL)
    {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    else if (parseBip32(&apdu_buf[OFFSET_CDATA],
                        &apdu_buf[OFFSET_LC],
                        &tmpCtx.messageSigningContext.bip32) != NULL)
    {
        if (!N_storage.verbose_eip712 && (ui_712_get_filtering_mode() == EIP712_FILTERING_BASIC))
        {
            ui_712_message_hash();
        }
        ret = true;
        ui_712_end_sign();
    }
    if (!ret)
    {
        handle_eip712_return_code(ret);
    }
    return ret;
}


#endif // HAVE_EIP712_FULL_SUPPORT
