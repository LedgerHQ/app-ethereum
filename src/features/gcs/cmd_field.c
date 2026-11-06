#include "cmd_field.h"
#include "cx.h"
#include "gcs_field.h"
#include "operation_ctx.h"
#include "app_mem_utils.h"

static bool process_field(const buffer_t *buf, s_field *field) {
    s_field_ctx ctx = {0};

    ctx.field = field;
    if (!handle_field_struct(buf, &ctx)) {
        PRINTF("Error: could not handle the field struct!\n");
        cleanup_field(field);
        return false;
    }
    if (!verify_field_struct(&ctx)) {
        PRINTF("Error: could not verify the field struct!\n");
        cleanup_field(field);
        return false;
    }
    // Hash before format_field so validate_instruction_hash() inside
    // add_to_field_table() sees the updated digest and sets end_intent on the
    // last field of a batch sub-transaction (drives the NBGL page-break).
    //
    // Finding 907: rollback the hash if format_field() rejects the field so the
    // rejected data leaves no trace in the signed digest.
    cx_sha3_t *hash_ctx_backup;
    cx_hash_t *hash_ctx = get_current_operation_fields_hash_ctx();
    if ((hash_ctx == NULL) ||
        (APP_MEM_CALLOC((void **) &hash_ctx_backup, sizeof(*hash_ctx_backup)) == false)) {
        cleanup_field(field);
        return false;
    }
    memcpy(hash_ctx_backup, hash_ctx, sizeof(*hash_ctx_backup));
    if (cx_hash_no_throw(hash_ctx, 0, buf->ptr, buf->size, NULL, 0) != CX_OK) {
        PRINTF("Error: could not hash the field struct!\n");
        APP_MEM_FREE(hash_ctx_backup);
        cleanup_field(field);
        return false;
    }
    if (!format_field(field, 0)) {
        memcpy(hash_ctx, hash_ctx_backup, sizeof(*hash_ctx_backup));
        APP_MEM_FREE(hash_ctx_backup);
        return false;
    }
    APP_MEM_FREE(hash_ctx_backup);
    return true;
}

bool handle_field_tlv_payload(const buffer_t *buf) {
    s_field *field = NULL;
    bool ret;

    // on the heap: the stack must hold the nested group recursion
    if (APP_MEM_CALLOC((void **) &field, sizeof(*field)) == false) {
        return false;
    }
    ret = process_field(buf, field);
    APP_MEM_FREE(field);
    return ret;
}
