#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include "field_hash.h"
#include "encode_field.h"
#include "path.h"
#include "mem.h"
#include "mem_utils.h"
#include "shared_context.h"
#include "ui_logic.h"
#include "context_712.h"     // contract_addr
#include "common_utils.h"    // u64_from_BE
#include "apdu_constants.h"  // APDU response codes
#include "typed_data.h"
#include "commands_712.h"
#include "hash_bytes.h"

static s_field_hashing *fh = NULL;

/**
 * Initialize the field hash context
 *
 * @return whether the initialization was successful or not
 */
bool field_hash_init(void) {
    if (fh == NULL) {
        if ((fh = MEM_ALLOC_AND_ALIGN_TYPE(*fh)) == NULL) {
            apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
            return false;
        }
        fh->state = FHS_IDLE;
    }
    return true;
}

/**
 * Deinitialize the field hash context
 */
void field_hash_deinit(void) {
    fh = NULL;
}

/**
 * Special handling of the first chunk received from a field value
 *
 * @param[in] field_ptr pointer to the struct field definition
 * @param[in] data the field value
 * @param[in,out] data_length the value length
 * @return the data pointer
 */
static const uint8_t *field_hash_prepare(const void *const field_ptr,
                                         const uint8_t *data,
                                         uint8_t *data_length) {
    e_type field_type;
    cx_err_t error = CX_INTERNAL_ERROR;

    field_type = struct_field_type(field_ptr);
    fh->remaining_size = __builtin_bswap16(*(uint16_t *) &data[0]);  // network byte order
    data += sizeof(uint16_t);
    *data_length -= sizeof(uint16_t);
    fh->state = FHS_WAITING_FOR_MORE;
    if (IS_DYN(field_type)) {
        CX_CHECK(cx_keccak_init_no_throw(&global_sha3, 256));
    }
    return data;
end:
    return NULL;
}

/**
 * Finalize static field hash
 *
 * Encode the field data depending on its type
 *
 * @param[in] field_ptr pointer to the struct field definition
 * @param[in] data the field value
 * @param[in] data_length the value length
 * @return pointer to the encoded value
 */
static const uint8_t *field_hash_finalize_static(const void *const field_ptr,
                                                 const uint8_t *const data,
                                                 uint8_t data_length) {
    uint8_t *value = NULL;
    e_type field_type;

    field_type = struct_field_type(field_ptr);
    switch (field_type) {
        case TYPE_SOL_INT:
            value = encode_int(data, data_length, get_struct_field_typesize(field_ptr));
            break;
        case TYPE_SOL_UINT:
            value = encode_uint(data, data_length);
            break;
        case TYPE_SOL_BYTES_FIX:
            value = encode_bytes(data, data_length);
            break;
        case TYPE_SOL_ADDRESS:
            value = encode_address(data, data_length);
            break;
        case TYPE_SOL_BOOL:
            value = encode_boolean((bool *) data, data_length);
            break;
        case TYPE_CUSTOM:
        default:
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            PRINTF("Unknown solidity type!\n");
    }
    return value;
}

/**
 * Finalize dynamic field hash
 *
 * Allocate and hash the data
 *
 * @return pointer to the hash, \ref NULL if it failed
 */
static uint8_t *field_hash_finalize_dynamic(void) {
    uint8_t *value;
    cx_err_t error = CX_INTERNAL_ERROR;

    if ((value = mem_alloc(KECCAK256_HASH_BYTESIZE)) == NULL) {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
        return NULL;
    }
    // copy hash into memory
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              CX_LAST,
                              NULL,
                              0,
                              value,
                              KECCAK256_HASH_BYTESIZE));
    return value;
end:
    return NULL;
}

/**
 * Feed the newly created field hash into the parent struct's progressive hash
 *
 * @param[in] field_type the struct field's type
 * @param[in] hash the field hash
 */
static void field_hash_feed_parent(e_type field_type, const uint8_t *const hash) {
    uint8_t len;

    if (IS_DYN(field_type)) {
        len = KECCAK256_HASH_BYTESIZE;
    } else {
        len = EIP_712_ENCODED_FIELD_LENGTH;
    }

    // last thing in mem is the hash of the previous field
    // and just before it is the current hash context
    cx_sha3_t *hash_ctx = (cx_sha3_t *) (hash - sizeof(cx_sha3_t));
    // continue the progressive hash on it
    hash_nbytes(hash, len, (cx_hash_t *) hash_ctx);
    // deallocate it
    mem_dealloc(len);
}

/**
 * Special domain fields handling
 *
 * Do something special for certain EIP712Domain fields
 *
 * @param[in] field_ptr pointer to the struct field definition
 * @param[in] data the field value
 * @param[in] data_length the value length
 * @return whether an error occurred or not
 */
static bool field_hash_domain_special_fields(const void *const field_ptr,
                                             const uint8_t *const data,
                                             uint8_t data_length) {
    const char *key;
    uint8_t keylen;
    const char *ethermint_vc = "cosmos";

    key = get_struct_field_keyname(field_ptr, &keylen);
    // copy contract address into context
    if (strncmp(key, "verifyingContract", keylen) == 0) {
        switch (struct_field_type(field_ptr)) {
            case TYPE_SOL_ADDRESS:
                if (data_length > sizeof(eip712_context->contract_addr)) {
                    apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                    PRINTF("Error: verifyingContract too big\n");
                    return false;
                }
                break;
            case TYPE_SOL_STRING:
                // hardcoded check for their non-standard implementation
                if ((data_length != strlen(ethermint_vc)) ||
                    (strncmp((char *) data, ethermint_vc, data_length) != 0)) {
                    apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                    PRINTF("Error: non standard verifyingContract\n");
                    return false;
                }
                break;
            default:
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                PRINTF("Error: unexpected type for verifyingContract (%u)!\n",
                       struct_field_type(field_ptr));
                return false;
        }
        memcpy(eip712_context->contract_addr, data, data_length);
        explicit_bzero(&eip712_context->contract_addr[data_length],
                       sizeof(eip712_context->contract_addr) - data_length);
    } else if (strncmp(key, "chainId", keylen) == 0) {
        eip712_context->chain_id = u64_from_BE(data, data_length);
    }
    return true;
}

/**
 * Finalize the data hashing
 *
 * @param[in] field_ptr pointer to the struct field definition
 * @param[in] data the field value
 * @param[in] data_length the value length
 * @return whether an error occurred or not
 */
static bool field_hash_finalize(const void *const field_ptr,
                                const uint8_t *const data,
                                uint8_t data_length) {
    const uint8_t *value = NULL;
    e_type field_type;

    field_type = struct_field_type(field_ptr);
    if (!IS_DYN(field_type)) {
        if ((value = field_hash_finalize_static(field_ptr, data, data_length)) == NULL) {
            return false;
        }
    } else {
        if ((value = field_hash_finalize_dynamic()) == NULL) {
            return false;
        }
    }

    field_hash_feed_parent(field_type, value);

    if (path_get_root_type() == ROOT_DOMAIN) {
        if (field_hash_domain_special_fields(field_ptr, data, data_length) == false) {
            return false;
        }
    }
    path_advance(true);
    fh->state = FHS_IDLE;
    ui_712_finalize_field();
    return true;
}

/**
 * Hash a field value
 *
 * @param[in] data the field value
 * @param[in] data_length the value length
 * @param[in] partial whether there is more of that data coming later or not
 * @return whether the data hashing was successful or not
 */
bool field_hash(const uint8_t *data, uint8_t data_length, bool partial) {
    const void *field_ptr;
    e_type field_type;
    bool first = fh->state == FHS_IDLE;

    if ((fh == NULL) || ((field_ptr = path_get_field()) == NULL)) {
        apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        return false;
    }

    field_type = struct_field_type(field_ptr);
    // first packet for this frame
    if (first) {
        if (!ui_712_show_raw_key(field_ptr)) {
            return false;
        }
        if (data_length < 2) {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
        }

        data = field_hash_prepare(field_ptr, data, &data_length);
    }
    if (data_length > fh->remaining_size) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return false;
    }
    fh->remaining_size -= data_length;
    // if a dynamic type -> continue progressive hash
    if (IS_DYN(field_type)) {
        hash_nbytes(data, data_length, (cx_hash_t *) &global_sha3);
    }
    if (!ui_712_feed_to_display(field_ptr, data, data_length, first, fh->remaining_size == 0)) {
        return false;
    }
    if (fh->remaining_size == 0) {
        if (partial)  // only makes sense if marked as complete
        {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
        }
        if (field_hash_finalize(field_ptr, data, data_length) == false) {
            return false;
        }
    } else {
        if (!partial || !IS_DYN(field_type))  // only makes sense if marked as partial
        {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
            return false;
        }
        handle_eip712_return_code(true);
    }
    return true;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
