#include <stdlib.h>
#include <string.h>
#include "tlv.h"
#include "os_print.h"  // PRINTF
#include "read.h"      // read_u32_be

typedef enum { TLV_STATE_TAG, TLV_STATE_LENGTH, TLV_STATE_VALUE } e_tlv_state;

#define DER_LONG_FORM_FLAG        0x80  // 8th bit set
#define DER_FIRST_BYTE_VALUE_MASK 0x7f

static bool parse_der_value(const uint8_t *payload,
                            uint16_t size,
                            uint16_t *offset,
                            uint32_t *value) {
    uint8_t byte_length;
    uint8_t buf[sizeof(*value)];

    if (value == NULL) {
        return false;
    }
    if (payload[*offset] & DER_LONG_FORM_FLAG) {  // long form
        byte_length = payload[*offset] & DER_FIRST_BYTE_VALUE_MASK;
        *offset += 1;
        if ((*offset + byte_length) > size) {
            PRINTF("TLV payload too small for DER encoded value\n");
            return false;
        }
        if ((byte_length > sizeof(buf)) || (byte_length == 0)) {
            PRINTF("Unexpectedly long DER-encoded value (%u bytes)\n", byte_length);
            return false;
        }
        memset(buf, 0, (sizeof(buf) - byte_length));
        memcpy(buf + (sizeof(buf) - byte_length), &payload[*offset], byte_length);
        *value = read_u32_be(buf, 0);
        *offset += byte_length;
    } else {  // short form
        *value = payload[*offset];
        *offset += 1;
    }
    return true;
}

/**
 * Parse a TLV payload
 *
 * @param[in] payload pointer to TLV payload
 * @param[in] size size of the given payload
 * @param[in] handler function to be called for each TLV data
 * @param[in,out] context structure passed to the handler function so the context from one handler
 * invocation to the next can be kept
 * @return whether the parsing (and handling) was successful or not
 */
bool tlv_parse(const uint8_t *payload, uint16_t size, f_tlv_data_handler handler, void *context) {
    e_tlv_state state = TLV_STATE_TAG;
    uint16_t offset = 0;
    uint32_t tmp_value;
    s_tlv_data data;

    while (offset < size) {
        switch (state) {
            case TLV_STATE_TAG:
                data.raw = &payload[offset];
                if (!parse_der_value(payload, size, &offset, &tmp_value) ||
                    (tmp_value > UINT8_MAX)) {
                    return false;
                }
                data.tag = (uint8_t) tmp_value;
                state = TLV_STATE_LENGTH;
                break;

            case TLV_STATE_LENGTH:
                if (!parse_der_value(payload, size, &offset, &tmp_value) ||
                    (tmp_value > UINT16_MAX)) {
                    return false;
                }
                data.length = (uint16_t) tmp_value;
                data.value = &payload[offset];
                state = (data.length > 0) ? TLV_STATE_VALUE : TLV_STATE_TAG;
                break;

            case TLV_STATE_VALUE:
                if ((offset + data.length) > size) {
                    PRINTF("Error: value would go beyond the TLV payload!\n");
                    return false;
                }
                offset += data.length;
                state = TLV_STATE_TAG;
                break;

            default:
                return false;
        }

        // back to TAG, call the handler function
        if ((state == TLV_STATE_TAG) && (handler != NULL)) {
            data.raw_size = (&payload[offset] - data.raw);
            if (!(*handler)(&data, context)) {
                PRINTF("Error: failed in handler of tag 0x%02x\n", data.tag);
                return false;
            }
        }
    }
    // make sure the payload didn't end prematurely
    return state == TLV_STATE_TAG;
}
