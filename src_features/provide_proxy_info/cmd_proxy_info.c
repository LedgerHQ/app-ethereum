#if defined(HAVE_EIP712_FULL_SUPPORT) || defined(HAVE_GENERIC_TX_PARSER)

#include "cmd_proxy_info.h"
#include "proxy_info.h"
#include "tlv_apdu.h"
#include "mem.h"
#include "apdu_constants.h"

static bool handle_tlv_payload(const uint8_t *payload, uint16_t size, bool to_free) {
    s_proxy_info_ctx ctx = {0};
    bool parsing_ret;

    cx_sha256_init(&ctx.struct_hash);
    parsing_ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_proxy_info_struct, &ctx);
    if (to_free) mem_dealloc(sizeof(size));
    if (!parsing_ret || !verify_proxy_info_struct(&ctx)) {
        return false;
    }
    return true;
}

uint16_t handle_proxy_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}

#endif
