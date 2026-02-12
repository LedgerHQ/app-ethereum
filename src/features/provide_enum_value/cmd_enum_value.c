#include "cmd_enum_value.h"
#include "apdu_constants.h"
#include "enum_value.h"
#include "tlv_apdu.h"

static bool handle_tlv_payload(const buffer_t *buf) {
    bool ret = false;
    s_enum_value_ctx ctx = {0};

    cx_sha256_init(&ctx.hash_ctx);
    ret = handle_enum_value_tlv_payload(buf, &ctx);
    if (!ret) return false;
    if (!verify_enum_value_struct(&ctx)) {
        return false;
    }
    return true;
}

uint16_t handle_enum_value(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
