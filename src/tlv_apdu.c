#include <string.h>
#include "tlv_apdu.h"
#include "read.h"
#include "mem.h"
#include "mem_utils.h"
#include "os_print.h"
#include "ui_utils.h"

static uint8_t *g_tlv_payload = NULL;
static uint16_t g_tlv_size = 0;
static uint16_t g_tlv_pos = 0;

static void reset_state(void) {
    mem_buffer_cleanup((void **) &g_tlv_payload);
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
            if ((g_tlv_payload = app_mem_alloc(g_tlv_size)) == NULL) {
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
        ret = (*handler)((g_tlv_payload != NULL) ? g_tlv_payload : &payload[offset], g_tlv_size);
        reset_state();
    }
    return ret;
}
