#include <string.h>
#include "os_print.h"
#include "read.h"
#include "app_mem_utils.h"
#include "tlv_apdu.h"

static struct {
    uint8_t *payload;
    uint16_t size;
    uint16_t pos;
    f_tlv_payload_handler handler_ptr;
} g_tlv;

static void reset_state(void) {
    APP_MEM_FREE(g_tlv.payload);
    explicit_bzero(&g_tlv, sizeof(g_tlv));
}

e_tlv_apdu_ret tlv_from_apdu(bool first_chunk,
                             uint8_t lc,
                             const uint8_t *payload,
                             f_tlv_payload_handler handler) {
    bool ret;
    uint8_t offset = 0;
    uint8_t chunk_length;

    if ((payload == NULL) || (handler == NULL)) {
        reset_state();
        return TLV_APDU_ERROR;
    }

    if (first_chunk) {
        if (g_tlv.payload != NULL) {
            PRINTF("Error: remnants from an incomplete TLV payload!\n");
            reset_state();
            return TLV_APDU_ERROR;
        }
        if ((offset + sizeof(g_tlv.size)) > lc) {
            return TLV_APDU_ERROR;
        }
        g_tlv.size = read_u16_be(payload, offset);
        offset += sizeof(g_tlv.size);
        g_tlv.pos = 0;

        if (g_tlv.size > (lc - offset)) {
            if ((g_tlv.payload = APP_MEM_ALLOC(g_tlv.size)) == NULL) {
                reset_state();
                return TLV_APDU_ERROR;
            }
        }
        g_tlv.handler_ptr = handler;
    } else {
        if (handler != g_tlv.handler_ptr) {
            PRINTF("Error: given handler does not match cached one!\n");
            reset_state();
            return TLV_APDU_ERROR;
        }
    }

    if (g_tlv.size == 0) {
        PRINTF("Error: zero-length TLV payload is invalid!\n");
        reset_state();
        return TLV_APDU_ERROR;
    }

    chunk_length = lc - offset;
    if ((g_tlv.pos + chunk_length) > g_tlv.size) {
        PRINTF("Error: TLV payload bigger than expected!\n");
        reset_state();
        return TLV_APDU_ERROR;
    }

    if (g_tlv.payload != NULL) {
        memcpy(g_tlv.payload + g_tlv.pos, payload + offset, chunk_length);
    }

    g_tlv.pos += chunk_length;

    if (g_tlv.pos == g_tlv.size) {
        // Create buffer_t for the complete payload
        buffer_t buf;
        if (g_tlv.payload != NULL) {
            // Multi-chunk case: use allocated buffer
            buf = (buffer_t){.ptr = g_tlv.payload, .size = g_tlv.size, .offset = 0};
        } else {
            // Single-chunk case: use APDU data directly
            buf = (buffer_t){.ptr = (uint8_t *) &payload[offset], .size = g_tlv.size, .offset = 0};
        }
        ret = (*handler)(&buf);
        reset_state();
        return ret ? TLV_APDU_SUCCESS : TLV_APDU_ERROR;
    }
    return TLV_APDU_PENDING;
}
