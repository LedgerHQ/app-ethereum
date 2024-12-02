#ifdef HAVE_ENUM_VALUE

#include <string.h>
#include "cmd_enum_value.h"
#include "apdu_constants.h"
#include "read.h"
#include "mem.h"
#include "enum_value.h"

static uint16_t g_tlv_size;
static uint16_t g_tlv_pos;
static uint8_t *g_tlv_payload;

uint16_t handle_enum_value(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    uint8_t offset = 0;
    s_enum_value_ctx ctx;

    (void)p2;
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
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (g_tlv_pos == g_tlv_size) {
        explicit_bzero(&ctx, sizeof(ctx));
        cx_sha256_init(&ctx.struct_hash);
        if (!parse_tlv(g_tlv_payload, g_tlv_size, (f_tlv_data_handler)&handle_enum_value_struct, &ctx)) {
            return APDU_RESPONSE_INVALID_DATA;
        }
        mem_dealloc(sizeof(g_tlv_size));
        if (!verify_enum_value_struct(&ctx)) {
            return APDU_RESPONSE_INVALID_DATA;
        }
    }
    return APDU_RESPONSE_OK;
}

#endif // HAVE_ENUM_VALUE
