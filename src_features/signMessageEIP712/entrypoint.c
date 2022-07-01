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

#ifdef HAVE_EIP712_TESTING_KEY
static const uint8_t EIP712_FEEDER_PUBLIC_KEY[] = {
    0x04, 0x4c, 0xca, 0x8f, 0xad, 0x49, 0x6a, 0xa5, 0x04, 0x0a, 0x00, 0xa7, 0xeb, 0x2f,
    0x5c, 0xc3, 0xb8, 0x53, 0x76, 0xd8, 0x8b, 0xa1, 0x47, 0xa7, 0xd7, 0x05, 0x4a, 0x99,
    0xc6, 0x40, 0x56, 0x18, 0x87, 0xfe, 0x17, 0xa0, 0x96, 0xe3, 0x6c, 0x3b, 0x52, 0x3b,
    0x24, 0x4f, 0x3e, 0x2f, 0xf7, 0xf8, 0x40, 0xae, 0x26, 0xc4, 0xe7, 0x7a, 0xd3, 0xbc,
    0x73, 0x9a, 0xf5, 0xde, 0x6f, 0x2d, 0x77, 0xa7, 0xb6
};
#endif // HAVE_EIP712_TESTING_KEY

// lib functions
const void *get_array_in_mem(const void *ptr, uint8_t *const array_size)
{
    *array_size = *(uint8_t*)ptr;
    return (ptr + 1);
}

const char *get_string_in_mem(const uint8_t *ptr, uint8_t *const string_length)
{
    return (char*)get_array_in_mem(ptr, string_length);
}

// ptr must point to the beginning of a struct field
static inline uint8_t get_struct_field_typedesc(const uint8_t *ptr)
{
    return *ptr;
}

// ptr must point to the beginning of a struct field
bool    struct_field_is_array(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & ARRAY_MASK);
}

// ptr must point to the beginning of a struct field
bool    struct_field_has_typesize(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & TYPESIZE_MASK);
}

// ptr must point to the beginning of a struct field
e_type  struct_field_type(const uint8_t *ptr)
{
    return (get_struct_field_typedesc(ptr) & TYPE_MASK);
}

// ptr must point to the beginning of a struct field
// TODO: Extra check inside or not
uint8_t get_struct_field_typesize(const uint8_t *ptr)
{
    return *(ptr + 1);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_custom_typename(const uint8_t *ptr,
                                                uint8_t *const length)
{
    ptr += 1; // skip TypeDesc
    return get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_typename(const uint8_t *ptr,
                                      uint8_t *const length)
{
    if (struct_field_type(ptr) == TYPE_CUSTOM)
    {
        return get_struct_field_custom_typename(ptr, length);
    }
    return get_struct_field_sol_typename(ptr, length);
}

// ptr must point to the beginning of a depth level
e_array_type struct_field_array_depth(const uint8_t *ptr,
                                      uint8_t *const array_size)
{
    if (*ptr == ARRAY_FIXED_SIZE)
    {
        *array_size = *(ptr + 1);
    }
    return *ptr;
}

// ptr must point to the beginning of a struct field level
const uint8_t *get_next_struct_field_array_lvl(const uint8_t *ptr)
{
    switch (*ptr)
    {
        case ARRAY_DYNAMIC:
            break;
        case ARRAY_FIXED_SIZE:
            ptr += 1;
            break;
        default:
            // should not be in here :^)
            break;
    }
    return ptr + 1;
}

// Skips TypeDesc and TypeSize/Length+TypeName
// Came to be since it is used in multiple functions
// TODO: Find better name
const uint8_t *struct_field_half_skip(const uint8_t *ptr)
{
    const uint8_t *field_ptr;
    uint8_t size;

    field_ptr = ptr;
    ptr += 1; // skip TypeDesc
    if (struct_field_type(field_ptr) == TYPE_CUSTOM)
    {
        get_string_in_mem(ptr, &size);
        ptr += (1 + size); // skip typename
    }
    else if (struct_field_has_typesize(field_ptr))
    {
        ptr += 1; // skip TypeSize
    }
    return ptr;
}

// ptr must point to the beginning of a struct field
const uint8_t *get_struct_field_array_lvls_array(const uint8_t *ptr,
                                                 uint8_t *const length)
{
    ptr = struct_field_half_skip(ptr);
    return get_array_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const char *get_struct_field_keyname(const uint8_t *ptr,
                                     uint8_t *const length)
{
    const uint8_t *field_ptr;
    uint8_t size;

    field_ptr = ptr;
    ptr = struct_field_half_skip(ptr);
    if (struct_field_is_array(field_ptr))
    {
        ptr = get_array_in_mem(ptr, &size);
        while (size-- > 0)
        {
            ptr = get_next_struct_field_array_lvl(ptr);
        }
    }
    return get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct field
const uint8_t *get_next_struct_field(const void *ptr)
{
    uint8_t length;

    ptr = (uint8_t*)get_struct_field_keyname(ptr, &length);
    return (ptr + length);
}

// ptr must point to the beginning of a struct
const char *get_struct_name(const uint8_t *ptr, uint8_t *const length)
{
    return (char*)get_string_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct
const uint8_t *get_struct_fields_array(const uint8_t *ptr,
                                       uint8_t *const length)
{
    uint8_t name_length;

    get_struct_name(ptr, &name_length);
    ptr += (1 + name_length); // skip length
    return get_array_in_mem(ptr, length);
}

// ptr must point to the beginning of a struct
const uint8_t *get_next_struct(const uint8_t *ptr)
{
    uint8_t fields_count;

    ptr = get_struct_fields_array(ptr, &fields_count);
    while (fields_count-- > 0)
    {
        ptr = get_next_struct_field(ptr);
    }
    return ptr;
}

// ptr must point to the size of the structs array
const uint8_t *get_structs_array(const uint8_t *ptr, uint8_t *const length)
{
    return get_array_in_mem(ptr, length);
}

// Finds struct with a given name
const uint8_t *get_structn(const uint8_t *const ptr,
                           const char *const name_ptr,
                           const uint8_t name_length)
{
    uint8_t structs_count;
    const uint8_t *struct_ptr;
    const char *name;
    uint8_t length;

    struct_ptr = get_structs_array(ptr, &structs_count);
    while (structs_count-- > 0)
    {
        name = get_struct_name(struct_ptr, &length);
        if ((name_length == length) && (memcmp(name, name_ptr, length) == 0))
        {
            return struct_ptr;
        }
        struct_ptr = get_next_struct(struct_ptr);
    }
    return NULL;
}

static inline const uint8_t *get_struct(const uint8_t *const ptr,
                                        const char *const name_ptr)
{
    return get_structn(ptr, name_ptr, strlen(name_ptr));
}
//

bool    set_struct_name(const uint8_t *const data)
{
    uint8_t *length_ptr;
    char *name_ptr;

    // increment number of structs
    *(eip712_context->structs_array) += 1;

    // copy length
    if ((length_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *length_ptr = data[OFFSET_LC];

    // copy name
    if ((name_ptr = mem_alloc(sizeof(char) * data[OFFSET_LC])) == NULL)
    {
        return false;
    }
    memmove(name_ptr, &data[OFFSET_CDATA], data[OFFSET_LC]);

    // initialize number of fields
    if ((eip712_context->current_struct_fields_array = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *(eip712_context->current_struct_fields_array) = 0;
    return true;
}

// TODO: Split this function
// TODO: Handle partial sends
bool    set_struct_field(const uint8_t *const data)
{
    uint8_t data_idx = OFFSET_CDATA;
    uint8_t *type_desc_ptr;
    uint8_t *type_size_ptr;
    uint8_t *typename_len_ptr;
    char *typename;
    uint8_t *array_levels_count;
    e_array_type *array_level;
    uint8_t *array_level_size;
    uint8_t *fieldname_len_ptr;
    char *fieldname_ptr;

    // increment number of struct fields
    *(eip712_context->current_struct_fields_array) += 1;

    // copy TypeDesc
    if ((type_desc_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *type_desc_ptr = data[data_idx++];

    // check TypeSize flag in TypeDesc
    if (*type_desc_ptr & TYPESIZE_MASK)
    {
        // copy TypeSize
        if ((type_size_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            return false;
        }
        *type_size_ptr = data[data_idx++];
    }
    else if ((*type_desc_ptr & TYPE_MASK) == TYPE_CUSTOM)
    {
        // copy custom struct name length
        if ((typename_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            return false;
        }
        *typename_len_ptr = data[data_idx++];

        // copy name
        if ((typename = mem_alloc(sizeof(char) * *typename_len_ptr)) == NULL)
        {
            return false;
        }
        memmove(typename, &data[data_idx], *typename_len_ptr);
        data_idx += *typename_len_ptr;
    }
    if (*type_desc_ptr & ARRAY_MASK)
    {
        if ((array_levels_count = mem_alloc(sizeof(uint8_t))) == NULL)
        {
            return false;
        }
        *array_levels_count = data[data_idx++];
        for (int idx = 0; idx < *array_levels_count; ++idx)
        {
            if ((array_level = mem_alloc(sizeof(uint8_t))) == NULL)
            {
                return false;
            }
            *array_level = data[data_idx++];
            switch (*array_level)
            {
                case ARRAY_DYNAMIC: // nothing to do
                    break;
                case ARRAY_FIXED_SIZE:
                    if ((array_level_size = mem_alloc(sizeof(uint8_t))) == NULL)
                    {
                        return false;
                    }
                    *array_level_size = data[data_idx++];
                    break;
                default:
                    // should not be in here :^)
                    break;
            }
        }
    }

    // copy length
    if ((fieldname_len_ptr = mem_alloc(sizeof(uint8_t))) == NULL)
    {
        return false;
    }
    *fieldname_len_ptr = data[data_idx++];

    // copy name
    if ((fieldname_ptr = mem_alloc(sizeof(char) * *fieldname_len_ptr)) == NULL)
    {
        return false;
    }
    memmove(fieldname_ptr, &data[data_idx], *fieldname_len_ptr);
    return true;
}


bool    handle_eip712_struct_def(const uint8_t *const apdu_buf)
{
    bool ret = true;

    if (eip712_context == NULL)
    {
        if (!eip712_context_init())
        {
            return false;
        }
    }
    switch (apdu_buf[OFFSET_P2])
    {
        case P2_NAME:
            ret = set_struct_name(apdu_buf);
            break;
        case P2_FIELD:
            ret = set_struct_field(apdu_buf);
            break;
        default:
            PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                    apdu_buf[OFFSET_P2],
                    apdu_buf[OFFSET_INS]);
            ret = false;
    }
    if (ret)
    {
        G_io_apdu_buffer[0] = 0x90;
        G_io_apdu_buffer[1] = 0x00;
    }
    else
    {
        G_io_apdu_buffer[0] = 0x6A;
        G_io_apdu_buffer[1] = 0x80;
    }
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    return ret;
}

bool    handle_eip712_struct_impl(const uint8_t *const apdu_buf)
{
    bool ret = true;
    bool reply_apdu = true;

    switch (apdu_buf[OFFSET_P2])
    {
        case P2_NAME:
            // set root type
            if (path_set_root((char*)&apdu_buf[OFFSET_CDATA],
                              apdu_buf[OFFSET_LC]))
            {
                if (N_storage.verbose_eip712)
                {
                    ui_712_review_struct(path_get_root());
                    reply_apdu = false;
                }
                ui_712_field_flags_reset();
            }
            else
            {
                ret = false;
            }
            break;
        case P2_FIELD:
            if (field_hash(&apdu_buf[OFFSET_CDATA],
                           apdu_buf[OFFSET_LC],
                           apdu_buf[OFFSET_P1] != P1_COMPLETE))
            {
                reply_apdu = false;
            }
            else
            {
                ret = false;
            }
            break;
        case P2_ARRAY:
            if (!path_new_array_depth(apdu_buf[OFFSET_CDATA]))
            {
                ret = false;
            }
            break;
        default:
            PRINTF("Unknown P2 0x%x for APDU 0x%x\n",
                   apdu_buf[OFFSET_P2],
                   apdu_buf[OFFSET_INS]);
            ret = false;
    }
    if (reply_apdu)
    {
        if (ret)
        {
            G_io_apdu_buffer[0] = 0x90;
            G_io_apdu_buffer[1] = 0x00;
        }
        else
        {
            G_io_apdu_buffer[0] = 0x6a;
            G_io_apdu_buffer[1] = 0x80;
        }
        io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
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
            return false;
        }
    }
    else // P1_FIELD_NAME
    {
        if (path_get_root_type() != ROOT_MESSAGE)
        {
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
            ret = false;
    }
    if (reply_apdu)
    {
        if (ret)
        {
            G_io_apdu_buffer[0] = 0x90;
            G_io_apdu_buffer[1] = 0x00;
        }
        else
        {
            G_io_apdu_buffer[0] = 0x6A;
            G_io_apdu_buffer[1] = 0x80;
        }
        io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    }
    return ret;
}

bool    handle_eip712_sign(const uint8_t *const apdu_buf)
{
    if (parseBip32(&apdu_buf[OFFSET_CDATA],
                   &apdu_buf[OFFSET_LC],
                   &tmpCtx.messageSigningContext.bip32) == NULL)
    {
        return false;
    }
    if (!N_storage.verbose_eip712 && (ui_712_get_filtering_mode() == EIP712_FILTERING_BASIC))
    {
        ui_712_message_hash();
    }
    ui_712_end_sign();
    return true;
}


#endif // HAVE_EIP712_FULL_SUPPORT
