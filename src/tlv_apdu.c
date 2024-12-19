#include <string.h>
#include "tlv_apdu.h"
#include "read.h"
#ifdef HAVE_DYN_MEM_ALLOC
#include "mem.h"
#endif
#include "os_print.h"

#ifdef HAVE_DYN_MEM_ALLOC
static uint8_t *g_tlv_payload = NULL;
static uint16_t g_tlv_size;
static uint16_t g_tlv_pos;
static bool g_dyn;
#endif

bool tlv_from_apdu(bool first_chunk,
                   uint8_t lc,
                   const uint8_t *payload,
                   f_tlv_payload_handler handler) {
#ifndef HAVE_DYN_MEM_ALLOC
    uint16_t g_tlv_size = 0;
    uint16_t g_tlv_pos = 0;
#endif
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
        if (g_tlv_size > (lc - offset)) {
#ifdef HAVE_DYN_MEM_ALLOC
            if (g_tlv_payload != NULL) {
                PRINTF("Error: remnants from an incomplete TLV payload!\n");
                mem_dealloc(g_tlv_size);
                g_tlv_payload = NULL;
                return false;
            }

            g_dyn = true;
            g_tlv_payload = mem_alloc(g_tlv_size);
        } else {
            g_dyn = false;
#else
            PRINTF(
                "Error: cannot handle a TLV payload accross multiple APDUs without dynamic memory "
                "allocator\n");
            return false;
#endif
        }
    }
#ifdef HAVE_DYN_MEM_ALLOC
    if (g_dyn && (g_tlv_payload == NULL)) {
        return false;
    }
#endif
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

#ifdef HAVE_DYN_MEM_ALLOC
    if (g_dyn) {
        memcpy(g_tlv_payload + g_tlv_pos, payload + offset, chunk_length);
    }
#endif

    g_tlv_pos += chunk_length;

    if (g_tlv_pos == g_tlv_size) {
#ifdef HAVE_DYN_MEM_ALLOC
        ret = (*handler)(g_dyn ? g_tlv_payload : &payload[offset], g_tlv_size, g_dyn);
        g_tlv_payload = NULL;
#else
        ret = (*handler)(&payload[offset], g_tlv_size, false);
#endif
    }
    return ret;
}
