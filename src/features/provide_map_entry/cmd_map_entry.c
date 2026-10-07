#include "cmd_map_entry.h"
#include "apdu_constants.h"
#include "map_entry.h"
#include "tlv_apdu.h"

static bool handle_tlv_payload(const buffer_t *buf) {
    s_map_entry_ctx ctx = {0};

    cx_sha256_init(&ctx.hash_ctx);
    if (!handle_map_entry_tlv_payload(buf, &ctx)) return false;
    return verify_map_entry_struct(&ctx);
}

uint16_t handle_map_entry(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    if ((p1 != P1_FIRST_CHUNK) && (p1 != P1_FOLLOWING_CHUNK)) {
        return SWO_WRONG_P1_P2;
    }
    if (p2 != 0) {
        return SWO_WRONG_P1_P2;
    }
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
