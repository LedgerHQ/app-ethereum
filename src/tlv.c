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

/** Parse DER-encoded value
 *
 * Parses a DER-encoded value (up to 4 bytes long)
 * https://en.wikipedia.org/wiki/X.690
 *
 * @param[in] payload the TLV payload
 * @param[in] payload_size the size of the payload
 * @param[in,out] offset the payload offset
 * @param[out] value the parsed value
 * @return whether it was successful
 */
static bool parse_der_value(const uint8_t *payload,
                            size_t payload_size,
                            size_t *offset,
                            uint32_t *value) {
    bool ret = false;
    uint8_t byte_length;
    uint8_t buf[sizeof(*value)];

    if (value != NULL) {
        if (payload[*offset] & DER_LONG_FORM_FLAG) {  // long form
            byte_length = payload[*offset] & DER_FIRST_BYTE_VALUE_MASK;
            *offset += 1;
            if ((*offset + byte_length) > payload_size) {
                PRINTF("TLV payload too small for DER encoded value\n");
            } else {
                if (byte_length > sizeof(buf) || byte_length == 0) {
                    PRINTF("Unexpectedly long DER-encoded value (%u bytes)\n", byte_length);
                } else {
                    memset(buf, 0, (sizeof(buf) - byte_length));
                    memcpy(buf + (sizeof(buf) - byte_length), &payload[*offset], byte_length);
                    *value = U4BE(buf, 0);
                    *offset += byte_length;
                    ret = true;
                }
            }
        } else {  // short form
            *value = payload[*offset];
            *offset += 1;
            ret = true;
        }
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
 * @return whether it was successful
 */
static bool get_der_value_as_uint8(const uint8_t *payload,
                                   size_t payload_size,
                                   size_t *offset,
                                   uint8_t *value) {
    bool ret = false;
    uint32_t tmp_value;

    if (value != NULL) {
        if (!parse_der_value(payload, payload_size, offset, &tmp_value)) {
            apdu_response_code = APDU_RESPONSE_INVALID_DATA;
        } else {
            if (tmp_value <= UINT8_MAX) {
                *value = tmp_value;
                ret = true;
            } else {
                apdu_response_code = APDU_RESPONSE_INVALID_DATA;
                PRINTF("TLV DER-encoded value larger than 8 bits\n");
            }
        }
    }
    return ret;
}

/**
 * Handle generating the hash of the TLV payload (minus the signature itself)
 *
 * @param[in] data the TLV data
 * @param[in,out] sig the signature context
 * @param[in] buf the raw TLV buffer
 * @param[in] buf_size size of the buffer
 */
static void handle_sign(const s_tlv_data *data,
                        s_tlv_sig *sig,
                        const uint8_t *buf,
                        size_t buf_size) {
    if (sig != NULL) {
        if (data->tag != sig->tag) {
            hash_nbytes(buf, buf_size, sig->ctx);
        }
    }
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
 * @param[in,out] sig the signature information (optional)
 * @return whether it was successful
 */
bool parse_tlv(const uint8_t *payload,
               size_t payload_size,
               f_tlv_handler *handler,
               s_tlv_handler_param *param,
               s_tlv_sig *sig) {
    e_tlv_step step = TLV_TAG;
    s_tlv_data data;
    size_t offset = 0;
    size_t tag_start_off;

    // handle TLV payload
    while (offset < payload_size) {
        switch (step) {
            case TLV_TAG:
                tag_start_off = offset;
                if (!get_der_value_as_uint8(payload, payload_size, &offset, &data.tag)) {
                    return false;
                }
                step = TLV_LENGTH;
                break;

            case TLV_LENGTH:
                if (!get_der_value_as_uint8(payload, payload_size, &offset, &data.length)) {
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
                offset += data.length;
                handle_sign(&data, sig, &payload[tag_start_off], offset - tag_start_off);
                step = TLV_TAG;
                break;

            default:
                return false;
        }
    }
    return true;
}
