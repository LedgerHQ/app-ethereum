#ifdef HAVE_GENERIC_TX_PARSER

#include "cmd_field.h"
#include "cx.h"
#include "apdu_constants.h"
#include "read.h" // read_u16_be
#include "mem.h"
#include "tlv.h"
#include "gtp_field.h"
#include "gtp_tx_info.h"

static uint16_t g_tlv_size;
static uint16_t g_tlv_pos;
static uint8_t *g_tlv_payload;

uint16_t handle_field(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    bool ret;
    uint8_t offset = 0;
    s_field field;
    s_field_ctx ctx;

    (void)p2;
    if (appState != APP_STATE_SIGNING_TX) {
        PRINTF("App not in TX signing mode!\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    switch (p1) {
        case P1_FIRST_CHUNK:
            if ((offset + sizeof(g_tlv_size)) > lc) {
                return APDU_RESPONSE_INVALID_DATA;
            }
            g_tlv_size = read_u16_be(payload, offset);
            offset += sizeof(g_tlv_size);
            g_tlv_pos = 0;
            if ((g_tlv_payload = mem_alloc(g_tlv_size)) == NULL) {
                return APDU_RESPONSE_INSUFFICIENT_MEMORY;
            }
            __attribute__((fallthrough));
        case P1_FOLLOWING_CHUNK:
            memcpy(g_tlv_payload + g_tlv_pos, payload + offset, lc - offset);
            g_tlv_pos += (lc - offset);
            break;
    }
    if (g_tlv_pos > g_tlv_size) {
        PRINTF("TLV payload bigger than expected!\n");
        mem_dealloc(g_tlv_size);
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (g_tlv_pos == g_tlv_size) {
        explicit_bzero(&field, sizeof(field));
        explicit_bzero(&ctx, sizeof(ctx));
        ctx.field = &field;
        if (!parse_tlv(g_tlv_payload, g_tlv_size, (f_tlv_data_handler)&handle_field_struct, &ctx)) return APDU_RESPONSE_INVALID_DATA;
        ret = cx_hash_no_throw(get_fields_hash_ctx(), 0, g_tlv_payload, g_tlv_size, NULL, 0) == CX_OK;
        mem_dealloc(g_tlv_size);
        if (!ret) return APDU_RESPONSE_UNKNOWN;
        if (!format_field(&field)) {
            PRINTF("Error while formatting the field\n");
            return APDU_RESPONSE_INVALID_DATA;
        }
    }
    return APDU_RESPONSE_OK;
}

#endif // HAVE_GENERIC_TX_PARSER
