#ifdef HAVE_GENERIC_TX_PARSER

#include "cmd_field.h"
#include "cx.h"
#include "apdu_constants.h"
#include "mem.h"
#include "tlv.h"
#include "tlv_apdu.h"
#include "gtp_field.h"
#include "cmd_tx_info.h"
#include "gtp_tx_info.h"

static bool handle_tlv_payload(const uint8_t *payload, uint16_t size, bool to_free) {
    s_field field = {0};
    s_field_ctx ctx = {0};

    ctx.field = &field;
    if (!tlv_parse(payload, size, (f_tlv_data_handler) &handle_field_struct, &ctx)) return false;
    if (cx_hash_no_throw(get_fields_hash_ctx(), 0, payload, size, NULL, 0) != CX_OK) return false;
    if (to_free) mem_dealloc(size);
    if (!verify_field_struct(&ctx)) {
        PRINTF("Error: could not verify the field struct!\n");
        return false;
    }
    if (!format_field(&field)) {
        PRINTF("Error while formatting the field\n");
        return false;
    }
    return true;
}

uint16_t handle_field(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if (appState != APP_STATE_SIGNING_TX) {
        PRINTF("App not in TX signing mode!\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }

    if (g_tx_info == NULL) {
        PRINTF("Error: Field received without a TX info!\n");
        gcs_cleanup();
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }

    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}

#endif  // HAVE_GENERIC_TX_PARSER
