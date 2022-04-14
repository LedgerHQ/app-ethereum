#include <stdlib.h>
#include <string.h>
#include "encode_field.h"
#include "mem.h"
#include "eip712.h"
#include "shared_context.h"


/**
 * Encode a field value to 32 bytes
 *
 * @param[in] value field value to encode
 * @param[in] length field length before encoding
 * @return encoded field value
 */
static void *field_encode(const uint8_t *const value, uint8_t length)
{
    uint8_t *padded_value;

    if (length > 32) // sanity check
    {
        return NULL;
    }
    // 0-pad the value to 32 bytes
    if ((padded_value = mem_alloc(EIP_712_ENCODED_FIELD_LENGTH)) != NULL)
    {
        explicit_bzero(padded_value, EIP_712_ENCODED_FIELD_LENGTH - length);
        for (uint8_t idx = 0; idx < length; ++idx)
        {
            padded_value[EIP_712_ENCODED_FIELD_LENGTH - (length - idx)] = value[idx];
        }
    }
    return padded_value;
}

/**
 * Encode an integer
 *
 * @param[in] value pointer to the "packed" integer received
 * @param[in] length its byte-length
 * @return the encoded (hashed) value
 */
void    *encode_integer(const uint8_t *const value, uint8_t length)
{
    // no length check here since it will be checked by field_encode
    return field_encode(value, length);
}

/**
 * Encode a boolean
 *
 * @param[in] value pointer to the boolean received
 * @param[in] length its byte-length
 * @return the encoded (hashed) value
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
 * @return the encoded (hashed) value
 */
void    *encode_address(const uint8_t *const value, uint8_t length)
{
    if (length != ADDRESS_LENGTH) // sanity check
    {
        return NULL;
    }
    return encode_integer(value, length);
}
