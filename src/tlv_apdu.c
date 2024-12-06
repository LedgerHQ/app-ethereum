#include <string.h>
#include "tlv_apdu.h"
#include "read.h"
#ifdef HAVE_DYN_MEM_ALLOC
#include "mem.h"
#endif
#include "os_print.h"

static uint8_t *g_tlv_payload = NULL;
static uint16_t g_tlv_size;
static uint16_t g_tlv_pos;
static bool g_dyn;

bool get_tlv_from_apdu(bool first_chunk, uint8_t lc, const uint8_t *payload, f_tlv_payload_handler handler) {
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
        g_dyn = g_tlv_size > (lc - offset);
        if (g_dyn) {
#ifdef HAVE_DYN_MEM_ALLOC
            g_tlv_payload = mem_alloc(g_tlv_size);
#else
            PRINTF("Error: cannot handle a TLV payload accross multiple APDUs without dynamic memory allocator\n");
            return false;
#endif
        }
    }
    if (g_dyn && (g_tlv_payload == NULL)) {
        return false;
    }
    chunk_length = lc - offset;
    if ((g_tlv_pos + chunk_length) > g_tlv_size) {
        PRINTF("TLV payload bigger than expected!\n");
#ifdef HAVE_DYN_MEM_ALLOC
        if (g_dyn) {
            mem_dealloc(g_tlv_size);
            g_tlv_payload = NULL;
        }
#endif
        return false;
    }
    if (g_dyn) {
        memcpy(g_tlv_payload + g_tlv_pos, payload + offset, chunk_length);
    }
    g_tlv_pos += chunk_length;

    if (g_tlv_pos == g_tlv_size) {
        ret = (*handler)(g_dyn ? g_tlv_payload : &payload[offset], g_tlv_size, g_dyn);
        g_tlv_payload = NULL;
    }
    return ret;
}
