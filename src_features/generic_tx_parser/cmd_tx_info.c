#include <string.h>
#include "cmd_tx_info.h"
#include "cx.h"
#include "apdu_constants.h"
#include "mem.h"
#include "gtp_tx_info.h"
#include "tlv.h"
#include "tlv_apdu.h"
#include "calldata.h"
#include "gtp_field_table.h"
#include "common_ui.h"
#include "list.h"

static bool handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    s_tx_info_ctx ctx = {0};
    bool parsing_ret;

    if ((ctx.tx_info = app_mem_alloc(sizeof(*ctx.tx_info))) == NULL) {
        return NULL;
    }
    explicit_bzero(ctx.tx_info, sizeof(*ctx.tx_info));
    cx_sha256_init((cx_sha256_t *) &ctx.struct_hash);
    parsing_ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_tx_info_struct, &ctx);
    if (!parsing_ret || !verify_tx_info_struct(&ctx)) {
        return false;
    }
    if (cx_sha3_init_no_throw(&ctx.tx_info->fields_hash_ctx, 256) != CX_OK) {
        return false;
    }

    push_new_tx_ctx(ctx.tx_info);
    return field_table_init();
}

uint16_t handle_tx_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if (appState != APP_STATE_SIGNING_TX) {
        PRINTF("App not in TX signing mode!\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}

void gcs_cleanup(void) {
    ui_gcs_cleanup();
    field_table_cleanup();
    tx_info_cleanup();
    calldata_cleanup();
}
