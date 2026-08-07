#include "cmd_field.h"
#include "cx.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "gtp_field.h"
#include "cmd_tx_info.h"
#include "gtp_tx_info.h"
#include "tx_ctx.h"

static bool handle_tlv_payload(const buffer_t *buf) {
    s_field field = {0};
    s_field_ctx ctx = {0};

    ctx.field = &field;
    if (!handle_field_struct(buf, &ctx)) {
        PRINTF("Error: could not handle the field struct!\n");
        cleanup_field(&field);
        return false;
    }
    if (!verify_field_struct(&ctx)) {
        PRINTF("Error: could not verify the field struct!\n");
        cleanup_field(&field);
        return false;
    }
    // Hash only after formatting succeeds: a field that fails formatting must
    // not contribute to the instruction hash, otherwise it would be absent from
    // the displayed review but still included in the signed digest.
    if (!format_field(&field, 0)) {
        return false;
    }
    if (cx_hash_no_throw(get_fields_hash_ctx(), 0, buf->ptr, buf->size, NULL, 0) != CX_OK) {
        PRINTF("Error: could not hash the field struct!\n");
        return false;
    }
    while (((appState == APP_STATE_SIGNING_EIP712) || !tx_ctx_is_root()) &&
           validate_instruction_hash()) {
        if (!process_empty_txs_after()) {
            return false;
        }
        tx_ctx_pop();
    }
    return true;
}

uint16_t handle_field(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if ((appState != APP_STATE_SIGNING_TX) && (appState != APP_STATE_SIGNING_EIP712)) {
        PRINTF("App not in TX signing mode!\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }

    if (get_current_tx_info() == NULL) {
        // Do NOT call gcs_cleanup() here: if a GCS review is currently shown,
        // gcs_cleanup() frees g_pairs/g_pairsList while NBGL still holds
        // pointers to them, causing a crash.
        // Cleanup is handled in the approve/reject callbacks and in reset_app_context().
        PRINTF("Error: Field received without a TX info!\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }

    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
