#ifndef _SIGN_TX_H_
#define _SIGN_TX_H_

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

customStatus_e customProcessor(txContext_t *context);
uint16_t finalize_parsing(const txContext_t *context);
void ux_approve_tx(bool fromPlugin);
void start_signature_flow(void);

uint16_t handle_parsing_status(parserStatus_e status);

uint16_t get_public_key(uint8_t *out, uint8_t outLength);
bool max_transaction_fee_to_string(const txInt256_t *BEGasPrice,
                                   const txInt256_t *BEGasLimit,
                                   char *displayBuffer,
                                   uint32_t displayBufferSize);
uint16_t get_network_as_string(char *out, size_t out_size);

#endif  // _SIGN_TX_H_
