#include "cmd_proxy_info.h"
#include "proxy_info.h"
#include "tlv_apdu.h"
#include "apdu_constants.h"
#include "challenge.h"

static bool handle_tlv_payload(const buffer_t *buf) {
    s_proxy_info_ctx ctx = {0};
    bool ret;

    proxy_cleanup();
    cx_sha256_init(&ctx.struct_hash);
    ret = handle_proxy_info_tlv_payload(buf, &ctx) && verify_proxy_info_struct(&ctx);
    // A challenge is good for a single attempt, successful or not
    roll_challenge();
    return ret;
}

uint16_t handle_proxy_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        proxy_cleanup();
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
