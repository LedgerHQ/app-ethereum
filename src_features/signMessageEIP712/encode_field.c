#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdlib.h>
#include <string.h>
#include "encode_field.h"
#include "mem.h"
#include "shared_context.h"
#include "apdu_constants.h"  // APDU response codes

typedef enum { MSB, LSB } e_padding_type;

/**
 * Encode a field value to 32 bytes (padded)
 *
 * @param[in] value field value to encode
 * @param[in] length field length before encoding
 * @param[in] ptype padding direction (LSB vs MSB)
 * @param[in] pval value used for padding
 * @return encoded field value
 */
static void *field_encode(const uint8_t *const value,
                          uint8_t length,
                          e_padding_type ptype,
                          uint8_t pval) {
    uint8_t *padded_value;
    uint8_t start_idx;

    if (length > EIP_712_ENCODED_FIELD_LENGTH)  // sanity check
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return NULL;
    }
    if ((padded_value = mem_alloc(EIP_712_ENCODED_FIELD_LENGTH)) != NULL) {
        switch (ptype) {
            case MSB:
                memset(padded_value, pval, EIP_712_ENCODED_FIELD_LENGTH - length);
                start_idx = EIP_712_ENCODED_FIELD_LENGTH - length;
                break;
            case LSB:
                explicit_bzero(padded_value + length, EIP_712_ENCODED_FIELD_LENGTH - length);
                start_idx = 0;
                break;
            default:
                apdu_response_code = APDU_RESPONSE_CONDITION_NOT_SATISFIED;
                return NULL;  // should not be here
        }
        memcpy(&padded_value[start_idx], value, length);
    } else {
        apdu_response_code = APDU_RESPONSE_INSUFFICIENT_MEMORY;
    }
    return padded_value;
}

/**
 * Encode an unsigned integer
 *
 * @param[in] value pointer to the "packed" integer received
 * @param[in] length its byte-length
 * @return the encoded value
 */
void *encode_uint(const uint8_t *const value, uint8_t length) {
    // no length check here since it will be checked by field_encode
    return field_encode(value, length, MSB, 0x00);
}

/**
 * Encode a signed integer
 *
 * @param[in] value pointer to the "packed" integer received
 * @param[in] length its byte-length
 * @param[in] typesize the type size in bytes
 * @return the encoded value
 */
void *encode_int(const uint8_t *const value, uint8_t length, uint8_t typesize) {
    uint8_t padding_value;

    if (length < 1) {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return NULL;
    }

    if ((length == typesize) && (value[0] & (1 << 7)))  // negative number
    {
        padding_value = 0xFF;
    } else {
        padding_value = 0x00;
    }
    // no length check here since it will be checked by field_encode
    return field_encode(value, length, MSB, padding_value);
}

/**
 * Encode a fixed-size byte array
 *
 * @param[in] value pointer to the "packed" bytes array
 * @param[in] length its byte-length
 * @return the encoded value
 */
void *encode_bytes(const uint8_t *const value, uint8_t length) {
    // no length check here since it will be checked by field_encode
    return field_encode(value, length, LSB, 0x00);
}

/**
 * Encode a boolean
 *
 * @param[in] value pointer to the boolean received
 * @param[in] length its byte-length
 * @return the encoded value
 */
void *encode_boolean(const bool *const value, uint8_t length) {
    if (length != 1)  // sanity check
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return NULL;
    }
    return encode_uint((uint8_t *) value, length);
}

/**
 * Encode an address
 *
 * @param[in] value pointer to the address received
 * @param[in] length its byte-length
 * @return the encoded value
 */
void *encode_address(const uint8_t *const value, uint8_t length) {
    if (length != ADDRESS_LENGTH)  // sanity check
    {
        apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        return NULL;
    }
    return encode_uint(value, length);
}

#endif  // HAVE_EIP712_FULL_SUPPORT
