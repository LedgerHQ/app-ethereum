#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "os.h"
#include "apdu_constants.h"
#include "hash_bytes.h"
#include "tlv.h"

#define DER_LONG_FORM_FLAG        0x80  // 8th bit set
#define DER_FIRST_BYTE_VALUE_MASK 0x7f

typedef enum { TLV_TAG, TLV_LENGTH, TLV_VALUE } e_tlv_step;

/** Decode DER-encoded value
 *
 * Decode a DER-encoded value (up to 4 bytes long)
 * https://en.wikipedia.org/wiki/X.690
 *
 * @param[in] payload the TLV payload
 * @param[in] payload_size the size of the payload
 * @param[out] value the decoded value
 * @return how many bytes were read from the payload, 0 if unsuccessful
 */
static size_t der_decode_value(const uint8_t *payload, size_t payload_size, uint32_t *value) {
    size_t ret = 0;
    uint8_t byte_length;
    uint8_t buf[sizeof(*value)];

    if (value != NULL) {
        if (payload[0] & DER_LONG_FORM_FLAG) {  // long form
            byte_length = payload[0] & DER_FIRST_BYTE_VALUE_MASK;
            ret += 1;
            if ((1 + byte_length) > payload_size) {
                PRINTF("TLV payload too small for DER encoded value\n");
                return 0;
            } else {
                if ((byte_length > sizeof(buf)) || (byte_length == 0)) {
                    PRINTF("Unexpectedly long DER-encoded value (%u bytes)\n", byte_length);
                    return 0;
                } else {
                    memset(buf, 0, (sizeof(buf) - byte_length));
                    memcpy(buf + (sizeof(buf) - byte_length), payload, byte_length);
                    *value = U4BE(buf, 0);
                    ret += byte_length;
                }
            }
        } else {  // short form
            *value = *payload;
            ret += 1;
        }
    }
    return ret;
}

/** DER-encode value
 *
 * Encode a value in DER (up to 4 bytes long)
 * https://en.wikipedia.org/wiki/X.690
 *
 * @param[out] payload the TLV payload
 * @param[in] payload_size the size of the payload
 * @param[in] value the value to encode
 * @return how many bytes were written in the payload, 0 if unsuccessful
 */
size_t der_encode_value(uint8_t *payload, size_t payload_size, uint32_t value) {
    size_t ret = 1;
    uint8_t buf[sizeof(value)];
    uint8_t byte_length = sizeof(buf);

    if ((payload == NULL) || (payload_size == 0)) {
        return 0;
    }
    if (value > DER_FIRST_BYTE_VALUE_MASK) {
        U4BE_ENCODE(buf, 0, value);
        for (int idx = 0; buf[idx] == 0; ++idx) {
            byte_length -= 1;
        }
        if ((1 + byte_length) > payload_size) {
            return 0;
        }
        payload[0] = DER_LONG_FORM_FLAG | byte_length;
        memcpy(&payload[1], &buf[sizeof(buf) - byte_length], byte_length);
        ret += byte_length;
    } else {
        payload[0] = (uint8_t) value;
    }
    return ret;
}

/**
 * Get DER-encoded value as an uint8
 *
 * Parses the value and checks if it fits in the given \ref uint8_t value
 *
 * @param[in] payload the TLV payload
 * @param[in] payload_size size of the payload
 * @param[in,out] offset
 * @param[out] value the parsed value
 * @return how many bytes in the payload the value took, 0 if unsuccessful
 */
static size_t get_der_value_as_uint8(const uint8_t *payload, size_t payload_size, uint8_t *value) {
    size_t ret = 0;
    uint32_t tmp_value;

    if (value != NULL) {
        ret = der_decode_value(payload, payload_size, &tmp_value);
        if (ret == 0) {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        } else {
            if (tmp_value <= UINT8_MAX) {
                *value = tmp_value;
            } else {
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                PRINTF("TLV DER-encoded value larger than 8 bits\n");
                return 0;
            }
        }
    }
    return ret;
}

/**
 * Parse the TLV payload
 *
 * Does the TLV parsing but also the hash of the payload.
 *
 * @param[in] payload the raw TLV payload
 * @param[in] payload_size size of payload
 * @param[in] handler TLV handler function
 * @param[in,out] param pointer to parameter passed to TLV handler
 * @return whether it was successful
 */
bool parse_tlv(const uint8_t *payload,
               size_t payload_size,
               f_tlv_handler *handler,
               s_tlv_handler_param *param) {
    e_tlv_step step = TLV_TAG;
    s_tlv_data data;
    size_t offset = 0;
    size_t incr;

    // handle TLV payload
    while (offset < payload_size) {
        switch (step) {
            case TLV_TAG:
                incr = get_der_value_as_uint8(&payload[offset], payload_size - offset, &data.tag);
                if (incr == 0) {
                    return false;
                }
                step = TLV_LENGTH;
                break;

            case TLV_LENGTH:
                incr =
                    get_der_value_as_uint8(&payload[offset], payload_size - offset, &data.length);
                if (incr == 0) {
                    return false;
                }
                step = TLV_VALUE;
                break;

            case TLV_VALUE:
                if (offset >= payload_size) {
                    return false;
                }
                data.value = &payload[offset];
                if (!handler(&data, param)) {
                    return false;
                }
                incr = data.length;
                step = TLV_TAG;
                break;

            default:
                return false;
        }
        offset += incr;
    }
    return true;
}
