#ifndef _SIGN_TX_H_
#define _SIGN_TX_H_

#include "shared_context.h"

typedef enum {

    PLUGIN_UI_INSIDE = 0,
    PLUGIN_UI_OUTSIDE

} plugin_ui_state_t;

customStatus_e customProcessor(txContext_t *context);
void finalizeParsing();
void prepareFeeDisplay();
void prepareNetworkDisplay();
void ux_approve_tx(bool fromPlugin);
void report_finalize_error(void);
void start_signature_flow(void);

#endif  // _SIGN_TX_H_
