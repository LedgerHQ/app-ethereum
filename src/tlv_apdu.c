#include <string.h>
#include "os_print.h"
#include "read.h"
#include "app_mem_utils.h"
#include "tlv_apdu.h"

static uint8_t *g_tlv_payload = NULL;
static uint16_t g_tlv_size = 0;
static uint16_t g_tlv_pos = 0;

static void reset_state(void) {
    APP_MEM_FREE_AND_NULL((void **) &g_tlv_payload);
    g_tlv_size = 0;
    g_tlv_pos = 0;
}

bool tlv_from_apdu(bool first_chunk,
                   uint8_t lc,
                   const uint8_t *payload,
                   f_tlv_payload_handler handler) {
    bool ret = true;
    uint8_t offset = 0;
    uint8_t chunk_length;

    if (first_chunk) {
        if ((offset + sizeof(g_tlv_size)) > lc) {
            return false;
        }
        g_tlv_size = read_u16_be(payload, offset);
        offset += sizeof(g_tlv_size);
        g_tlv_pos = 0;
        if (g_tlv_payload != NULL) {
            PRINTF("Error: remnants from an incomplete TLV payload!\n");
            reset_state();
            return false;
        }

        if (g_tlv_size > (lc - offset)) {
            if ((g_tlv_payload = APP_MEM_ALLOC(g_tlv_size)) == NULL) {
                reset_state();
                return false;
            }
        }
    }
    chunk_length = lc - offset;
    if ((g_tlv_pos + chunk_length) > g_tlv_size) {
        PRINTF("TLV payload bigger than expected!\n");
        reset_state();
        return false;
    }

    if (g_tlv_payload != NULL) {
        memcpy(g_tlv_payload + g_tlv_pos, payload + offset, chunk_length);
    }

    g_tlv_pos += chunk_length;

    if (g_tlv_pos == g_tlv_size) {
        // Create buffer_t for the complete payload
        buffer_t buf;
        if (g_tlv_payload != NULL) {
            // Multi-chunk case: use allocated buffer
            buf = (buffer_t){.ptr = g_tlv_payload, .size = g_tlv_size, .offset = 0};
        } else {
            // Single-chunk case: use APDU data directly
            buf = (buffer_t){.ptr = (uint8_t *) &payload[offset], .size = g_tlv_size, .offset = 0};
        }
        ret = (*handler)(&buf);
        reset_state();
    }
    return ret;
}
