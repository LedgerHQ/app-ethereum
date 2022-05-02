#include <stdlib.h>
#include <string.h>
#include "encode_field.h"
#include "mem.h"
#include "eip712.h"
#include "shared_context.h"

typedef enum
{
    MSB,
    LSB
}   e_padding_type;

/**
 * Encode a field value to 32 bytes (0 padded)
 *
 * @param[in] value field value to encode
 * @param[in] length field length before encoding
 * @return encoded field value
 */
static void *field_encode(const uint8_t *const value, uint8_t length, e_padding_type ptype)
{
    uint8_t *padded_value;
    uint8_t start_idx;

    if (length > 32) // sanity check
    {
        return NULL;
    }
    // 0-pad the value to 32 bytes
    if ((padded_value = mem_alloc(EIP_712_ENCODED_FIELD_LENGTH)) != NULL)
    {
        switch (ptype)
        {
            case MSB:
                explicit_bzero(padded_value, EIP_712_ENCODED_FIELD_LENGTH - length);
                start_idx = EIP_712_ENCODED_FIELD_LENGTH - length;
                break;
            case LSB:
                explicit_bzero(padded_value + length, EIP_712_ENCODED_FIELD_LENGTH - length);
                start_idx = 0;
                break;
            default:
                return NULL; // should not be here
        }
        for (uint8_t idx = 0; idx < length; ++idx)
        {
            padded_value[start_idx + idx] = value[idx];
        }
    }
    return padded_value;
}

/**
 * Encode an integer
 *
 * @param[in] value pointer to the "packed" integer received
 * @param[in] length its byte-length
 * @return the encoded value
 */
void    *encode_integer(const uint8_t *const value, uint8_t length)
{
    // no length check here since it will be checked by field_encode
    return field_encode(value, length, MSB);
}

/**
 * Encode a fixed-size byte array
 *
 * @param[in] value pointer to the "packed" bytes array
 * @param[in] length its byte-length
 * @return the encoded value
 */
void    *encode_bytes(const uint8_t *const value, uint8_t length)
{
    // no length check here since it will be checked by field_encode
    return field_encode(value, length, LSB);
}

/**
 * Encode a boolean
 *
 * @param[in] value pointer to the boolean received
 * @param[in] length its byte-length
 * @return the encoded value
 */
void    *encode_boolean(const bool *const value, uint8_t length)
{
    if (length != 1) // sanity check
    {
        return NULL;
    }
    return encode_integer((uint8_t*)value, length);
}

/**
 * Encode an address
 *
 * @param[in] value pointer to the address received
 * @param[in] length its byte-length
 * @return the encoded value
 */
void    *encode_address(const uint8_t *const value, uint8_t length)
{
    if (length != ADDRESS_LENGTH) // sanity check
    {
        return NULL;
    }
    return encode_integer(value, length);
}
