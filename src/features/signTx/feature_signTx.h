#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ethUstream.h"

// App codes for detail.
typedef enum {
    APP_CODE_DEFAULT = 0x00,
    APP_CODE_CALLDATA_ISSUE = 0x01,
    APP_CODE_NO_STANDARD_UI = 0x02
} app_code_t;

typedef enum {

    PLUGIN_UI_INSIDE = 0,
    PLUGIN_UI_OUTSIDE

} plugin_ui_state_t;

extern cx_sha3_t *g_tx_hash_ctx;

customStatus_e customProcessor(txContext_t *context);
uint16_t finalize_parsing(const txContext_t *context);
void ux_approve_tx(bool fromPlugin);
void start_signature_flow(void);

uint16_t handle_parsing_status(parserStatus_e status);

bool max_transaction_fee_to_string(const txInt256_t *BEGasPrice,
                                   const txInt256_t *BEGasLimit,
                                   char *displayBuffer,
                                   uint32_t displayBufferSize);
