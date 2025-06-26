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
#include "ui_utils.h"
#include "mem_utils.h"

static bool handle_tlv_payload(const uint8_t *payload, uint16_t size) {
    s_tx_info_ctx ctx = {0};
    bool parsing_ret;

    ctx.tx_info = g_tx_info;
    explicit_bzero(ctx.tx_info, sizeof(*ctx.tx_info));
    cx_sha256_init((cx_sha256_t *) &ctx.struct_hash);
    parsing_ret = tlv_parse(payload, size, (f_tlv_data_handler) &handle_tx_info_struct, &ctx);
    if (!parsing_ret || !verify_tx_info_struct(&ctx)) {
        return false;
    }
    if (cx_sha3_init_no_throw(&ctx.tx_info->fields_hash_ctx, 256) != CX_OK) {
        return false;
    }
    return field_table_init();
}

uint16_t handle_tx_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if (appState != APP_STATE_SIGNING_TX) {
        PRINTF("App not in TX signing mode!\n");
        return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    }

    if (p1 == P1_FIRST_CHUNK) {
        if (g_tx_info != NULL) {
            PRINTF("Error: TX info received when one was still in RAM!\n");
            gcs_cleanup();
            return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
        }

        if ((g_tx_info = app_mem_alloc(sizeof(*g_tx_info))) == NULL) {
            return APDU_RESPONSE_INSUFFICIENT_MEMORY;
        }
    }
    if (g_tx_info == NULL) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    return APDU_RESPONSE_OK;
}

void gcs_cleanup(void) {
    ui_gcs_cleanup();
    field_table_cleanup();
    mem_buffer_cleanup((void **) &g_tx_info);
    calldata_cleanup();
}
