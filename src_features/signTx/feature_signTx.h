#include "shared_context.h"

typedef enum {

    PLUGIN_UI_INSIDE = 0,
    PLUGIN_UI_OUTSIDE

} plugin_ui_state_t;

customStatus_e customProcessor(txContext_t *context);
void finalizeParsing(bool direct);
void prepareFeeDisplay();
void prepareChainIdDisplay();
void ux_approve_tx(bool fromPlugin);
