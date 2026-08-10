#include "cmd_field.h"
#include "cx.h"
#include "apdu_constants.h"
#include "tlv_apdu.h"
#include "gtp_field.h"
#include "cmd_tx_info.h"
#include "gtp_tx_info.h"
#include "tx_ctx.h"
#include "app_mem_utils.h"

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
    // Hash before format_field so validate_instruction_hash() inside
    // add_to_field_table() sees the updated digest and sets end_intent on the
    // last field of a batch sub-transaction (drives the NBGL page-break).
    //
    // Finding 907: rollback the hash if format_field() rejects the field so the
    // rejected data leaves no trace in the signed digest.
    cx_sha3_t *hash_ctx_backup;
    if (APP_MEM_CALLOC((void **) &hash_ctx_backup, sizeof(*hash_ctx_backup)) == false) {
        cleanup_field(&field);
        return false;
    }
    memcpy(hash_ctx_backup, get_fields_hash_ctx(), sizeof(*hash_ctx_backup));
    if (cx_hash_no_throw(get_fields_hash_ctx(), 0, buf->ptr, buf->size, NULL, 0) != CX_OK) {
        PRINTF("Error: could not hash the field struct!\n");
        APP_MEM_FREE(hash_ctx_backup);
        cleanup_field(&field);
        return false;
    }
    if (!format_field(&field, 0)) {
        memcpy(get_fields_hash_ctx(), hash_ctx_backup, sizeof(*hash_ctx_backup));
        APP_MEM_FREE(hash_ctx_backup);
        return false;
    }
    APP_MEM_FREE(hash_ctx_backup);
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
