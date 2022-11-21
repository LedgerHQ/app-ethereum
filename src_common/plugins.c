#include "eth_plugin_handler.h"
#include "ui_callbacks.h"

void plugin_ui_get_id(void) {
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

void plugin_ui_get_item_internal(char *title_buffer,
                                 size_t title_buffer_size,
                                 char *msg_buffer,
                                 size_t msg_buffer_size) {
    ethQueryContractUI_t pluginQueryContractUI;
    eth_plugin_prepare_query_contract_UI(&pluginQueryContractUI,
                                         dataContext.tokenContext.pluginUiCurrentItem,
                                         title_buffer,
                                         title_buffer_size,
                                         msg_buffer,
                                         msg_buffer_size);
    if (!eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, (void *) &pluginQueryContractUI)) {
        PRINTF("Plugin query contract UI call failed\n");
        io_seproxyhal_touch_tx_cancel(NULL);
    }
}

void plugin_ui_get_item(void) {
    plugin_ui_get_item_internal(strings.common.fullAddress,
                                sizeof(strings.common.fullAddress),
                                strings.common.fullAmount,
                                sizeof(strings.common.fullAmount));
}