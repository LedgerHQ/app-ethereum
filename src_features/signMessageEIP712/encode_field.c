#include <stdlib.h>
#include <string.h>
#include "encode_field.h"
#include "mem.h"
#include "eip712.h"
#include "shared_context.h"


/**
 * Hash field value
 *
 * @param[in] value pointer to value
 * @param[in] length its bytelength
 * @param[in] dealloc if the value length should be deallocated from the memory
 */
static void *hash_field_value(const void *const value, uint16_t length, bool dealloc)
{
    uint8_t *hash_ptr = NULL;

    if (value != NULL)
    {
        cx_keccak_init((cx_hash_t*)&global_sha3, 256);
        cx_hash((cx_hash_t*)&global_sha3,
                0,
                (uint8_t*)value,
                length,
                NULL,
                0);

        if (dealloc)
        {
            // restore the memory location
            mem_dealloc(length);
        }

        if ((hash_ptr = mem_alloc(KECCAK256_HASH_BYTESIZE)) == NULL)
        {
            return NULL;
        }

        // copy hash into memory
        cx_hash((cx_hash_t*)&global_sha3,
                CX_LAST,
                NULL,
                0,
                hash_ptr,
                KECCAK256_HASH_BYTESIZE);
    }
    return hash_ptr;
}

/**
 * Encode an integer and hash it
 *
 * @param[in] value pointer to the "packed" integer received
 * @param[in] length its byte-length
 * @return the encoded (hashed) value
 */
void    *encode_integer(const uint8_t *const value, uint16_t length)
{
    uint8_t *padded_value;

    // 0-pad the value to 32 bytes
    if ((padded_value = mem_alloc(EIP_712_ENCODED_FIELD_LENGTH)) != NULL)
    {
        explicit_bzero(padded_value, EIP_712_ENCODED_FIELD_LENGTH - length);
        for (uint8_t idx = 0; idx < length; ++idx)
        {
            padded_value[EIP_712_ENCODED_FIELD_LENGTH - (length - idx)] = value[idx];
        }
    }
    return hash_field_value(padded_value, EIP_712_ENCODED_FIELD_LENGTH, true);
}

/**
 * Encode a string and hash it
 *
 * @param[in] value pointer to the string received
 * @param[in] length its byte-length
 * @return the encoded (hashed) value
 */
void    *encode_string(const char *const value, uint16_t length)
{
    return hash_field_value(value, length, false);
}

/**
 * Encode a boolean and hash it
 *
 * @param[in] value pointer to the boolean received
 * @param[in] length its byte-length
 * @return the encoded (hashed) value
 */
void    *encode_bool(const bool *const value, uint16_t length)
{
    if (length != 1)
    {
        return NULL;
    }
    return encode_integer((uint8_t*)value, length);
}

/**
 * Encode an address and hash it
 *
 * @param[in] value pointer to the address received
 * @param[in] length its byte-length
 * @return the encoded (hashed) value
 */
void    *encode_address(const uint8_t *const value, uint16_t length)
{
    if (length != ADDRESS_LENGTH)
    {
        return NULL;
    }
    return encode_integer(value, length);
}

/**
 * Encode bytes and hash it
 *
 * @param[in] value pointer to the bytes received
 * @param[in] length its byte-length
 * @return the encoded (hashed) value
 */
void    *encode_bytes(const uint8_t *const value, uint16_t length)
{
    return hash_field_value(value, length, false);
}
