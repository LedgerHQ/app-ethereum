#ifdef HAVE_ENUM_VALUE

#include <string.h>
#include "cmd_enum_value.h"
#include "apdu_constants.h"
#include "read.h"
#include "mem.h"
#include "enum_value.h"
#include "tlv_apdu.h"
#include "tlv.h"

bool handle_tlv_payload(const uint8_t *payload, uint16_t size, bool to_free) {
    s_enum_value_ctx ctx = {0};

    cx_sha256_init(&ctx.struct_hash);
    if (!tlv_parse(payload, size, (f_tlv_data_handler) &handle_enum_value_struct, &ctx)) {
        return false;
    }
    if (to_free) mem_dealloc(sizeof(size));
    if (!verify_enum_value_struct(&ctx)) {
        return false;
    }
    return true;
}

uint16_t handle_enum_value(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}

#endif  // HAVE_ENUM_VALUE
