#include "feature_signTx.h"
#include "ux.h"
#include "eth_plugin_handler.h"
#include "ui_callbacks.h"
#include "ui_plugin.h"
#include "plugins.h"

// This function is not exported by the SDK
void ux_layout_paging_redisplay_by_addr(unsigned int stack_slot);

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
