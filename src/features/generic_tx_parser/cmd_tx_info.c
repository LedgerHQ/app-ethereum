#include <string.h>
#include "cmd_tx_info.h"
#include "cx.h"
#include "apdu_constants.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "gtp_tx_info.h"
#include "tlv_apdu.h"
#include "tx_ctx.h"

static bool handle_tlv_payload(const buffer_t *buf) {
    s_tx_info_ctx ctx = {0};

    if (APP_MEM_CALLOC((void **) &ctx.tx_info, sizeof(*ctx.tx_info)) == false) {
        return false;
    }
    cx_sha256_init(&ctx.struct_hash);
    if (!handle_tx_info_struct(buf, &ctx)) {
        APP_MEM_FREE(ctx.tx_info);
        return false;
    }
    if (!verify_tx_info_struct(&ctx)) {
        APP_MEM_FREE(ctx.tx_info);
        return false;
    }
    if (!find_matching_tx_ctx(ctx.tx_info->contract_addr,
                              ctx.tx_info->selector,
                              &ctx.tx_info->chain_id)) {
        APP_MEM_FREE(ctx.tx_info);
        return false;
    }
    return process_empty_txs_before() && set_tx_info_into_tx_ctx(ctx.tx_info);
}

uint16_t handle_tx_info(uint8_t p1, uint8_t p2, uint8_t lc, const uint8_t *payload) {
    (void) p2;
    if ((appState != APP_STATE_SIGNING_TX) && (appState != APP_STATE_SIGNING_EIP712)) {
        PRINTF("App not in TX signing mode!\n");
        return SWO_COMMAND_NOT_ALLOWED;
    }
    if (!tlv_from_apdu(p1 == P1_FIRST_CHUNK, lc, payload, &handle_tlv_payload)) {
        return SWO_INCORRECT_DATA;
    }
    return SWO_SUCCESS;
}
