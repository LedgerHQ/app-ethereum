#include "shared_context.h"
#include "eth_plugin_handler.h"
#include "ux.h"
#include "feature_signTx.h"

void plugin_ui_start() {
    dataContext.tokenContext.pluginUiState = PLUGIN_UI_OUTSIDE;
    dataContext.tokenContext.pluginUiCurrentItem = 0;

    ux_approve_tx(true);
}
