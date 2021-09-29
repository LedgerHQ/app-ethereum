#include "feature_signTx.h"
#include "ux.h"
#include "eth_plugin_handler.h"
#include "ui_callbacks.h"
#include "ui_plugin.h"

#ifdef TARGET_NANOS
// This function is not exported by the SDK
void ux_layout_paging_redisplay_by_addr(unsigned int stack_slot);
#endif

void plugin_ui_get_id() {
    ethQueryContractID_t pluginQueryContractID;
    eth_plugin_prepare_query_contract_ID(&pluginQueryContractID,
                                         strings.common.fullAddress,
                                         sizeof(strings.common.fullAddress),
                                         strings.common.fullAmount,
                                         sizeof(strings.common.fullAmount));
    // Query the original contract for ID if it's not an internal alias
    if (!eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, (void *) &pluginQueryContractID)) {
        PRINTF("Plugin query contract ID call failed\n");
        io_seproxyhal_touch_tx_cancel(NULL);
    }
}

void plugin_ui_get_item() {
    ethQueryContractUI_t pluginQueryContractUI;
    eth_plugin_prepare_query_contract_UI(&pluginQueryContractUI,
                                         dataContext.tokenContext.pluginUiCurrentItem,
                                         strings.common.fullAddress,
                                         sizeof(strings.common.fullAddress),
                                         strings.common.fullAmount,
                                         sizeof(strings.common.fullAmount));
    if (!eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, (void *) &pluginQueryContractUI)) {
        PRINTF("Plugin query contract UI call failed\n");
        io_seproxyhal_touch_tx_cancel(NULL);
    }
}

void display_next_plugin_item(bool entering) {
    if (entering) {
        if (dataContext.tokenContext.pluginUiState == PLUGIN_UI_OUTSIDE) {
            dataContext.tokenContext.pluginUiState = PLUGIN_UI_INSIDE;
            dataContext.tokenContext.pluginUiCurrentItem = 0;
            plugin_ui_get_item();
            ux_flow_next();
        } else {
            if (dataContext.tokenContext.pluginUiCurrentItem > 0) {
                dataContext.tokenContext.pluginUiCurrentItem--;
                plugin_ui_get_item();
                ux_flow_next();
            } else {
                dataContext.tokenContext.pluginUiState = PLUGIN_UI_OUTSIDE;
                dataContext.tokenContext.pluginUiCurrentItem = 0;
                ux_flow_prev();
            }
        }
    } else {
        if (dataContext.tokenContext.pluginUiState == PLUGIN_UI_OUTSIDE) {
            dataContext.tokenContext.pluginUiState = PLUGIN_UI_INSIDE;
            plugin_ui_get_item();
            ux_flow_prev();
        } else {
            if (dataContext.tokenContext.pluginUiCurrentItem <
                dataContext.tokenContext.pluginUiMaxItems - 1) {
                dataContext.tokenContext.pluginUiCurrentItem++;
                plugin_ui_get_item();
                ux_flow_prev();
                // Reset multi page layout to the first page
                G_ux.layout_paging.current = 0;
                ux_layout_paging_redisplay_by_addr(G_ux.stack_count - 1);
            } else {
                dataContext.tokenContext.pluginUiState = PLUGIN_UI_OUTSIDE;
                ux_flow_next();
            }
        }
    }
}