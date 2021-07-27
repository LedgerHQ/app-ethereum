#include "shared_context.h"
#ifdef HAVE_UX_FLOW
#include "ui_flow.h"
#endif
#include "ui_callbacks.h"
#include "eth_plugin_handler.h"
#include "ux.h"

typedef enum {

    PLUGIN_UI_INSIDE = 0,
    PLUGIN_UI_OUTSIDE

} plugin_ui_state_t;

#ifdef TARGET_NANOS
// This function is not exported by the SDK
void ux_layout_paging_redisplay_by_addr(unsigned int stack_slot);
#endif

void computeFees(char *displayBuffer, uint32_t displayBufferSize);

void plugin_ui_get_id() {
    ethQueryContractID_t pluginQueryContractID;
    eth_plugin_prepare_query_contract_ID(&pluginQueryContractID,
                                         strings.tmp.tmp,
                                         sizeof(strings.tmp.tmp),
                                         strings.tmp.tmp2,
                                         sizeof(strings.tmp.tmp2));
    // Query the original contract for ID if it's not an internal alias
    eth_plugin_result_t status =
        eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_ID, (void *) &pluginQueryContractID);
    if (status != ETH_PLUGIN_RESULT_OK) {
        PRINTF("Plugin query contract ID call failed\n");
        io_seproxyhal_touch_tx_cancel(NULL);
    }
}

void plugin_ui_get_item() {
    ethQueryContractUI_t pluginQueryContractUI;
    eth_plugin_prepare_query_contract_UI(&pluginQueryContractUI,
                                         dataContext.tokenContext.pluginUiCurrentItem,
                                         strings.tmp.tmp,
                                         sizeof(strings.tmp.tmp),
                                         strings.tmp.tmp2,
                                         sizeof(strings.tmp.tmp2));
    eth_plugin_result_t status =
        eth_plugin_call(ETH_PLUGIN_QUERY_CONTRACT_UI, (void *) &pluginQueryContractUI);
    if (status != ETH_PLUGIN_RESULT_OK) {
        PRINTF("Plugin query contract UI call failed, got: %d\n", status);
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
#ifdef TARGET_NANOS
                ux_layout_paging_redisplay_by_addr(G_ux.stack_count - 1);
#else
                ux_layout_bnnn_paging_redisplay(0);
#endif
            } else {
                dataContext.tokenContext.pluginUiState = PLUGIN_UI_OUTSIDE;
                ux_flow_next();
            }
        }
    }
}

void plugin_ui_compute_fees() {
    computeFees(strings.common.maxFee, sizeof(strings.common.maxFee));
}

// clang-format off
UX_FLOW_DEF_NOCB(
		ux_plugin_approval_intro_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "contract call",
    });

UX_STEP_NOCB_INIT(
  ux_plugin_approval_id_step,
  bnnn_paging,
  plugin_ui_get_id(),
  {
    .title = strings.tmp.tmp,
    .text = strings.tmp.tmp2
  });

UX_STEP_INIT(
  ux_plugin_approval_before_step,
  NULL,
  NULL,
  {
    display_next_plugin_item(true);
  });

UX_FLOW_DEF_NOCB(
  ux_plugin_approval_display_step,
  bnnn_paging,
  {
    .title = strings.tmp.tmp,
    .text = strings.tmp.tmp2
  });

UX_STEP_INIT(
  ux_plugin_approval_after_step,
  NULL,
  NULL,
  {
    display_next_plugin_item(false);
  });

UX_STEP_NOCB_INIT(
  ux_plugin_approval_fees_step,
  bnnn_paging,
  plugin_ui_compute_fees(),
  {
    .title = "Max Fees",
    .text = strings.common.maxFee
  });

UX_FLOW_DEF_VALID(
    ux_plugin_approval_ok_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_FLOW_DEF_VALID(
    ux_plugin_approval_cancel_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_plugin_approval_flow,
        &ux_plugin_approval_intro_step,
        &ux_plugin_approval_id_step,
        &ux_plugin_approval_before_step,
        &ux_plugin_approval_display_step,
        &ux_plugin_approval_after_step,
        &ux_plugin_approval_fees_step,
        &ux_plugin_approval_ok_step,
        &ux_plugin_approval_cancel_step);

void plugin_ui_start() {
    dataContext.tokenContext.pluginUiState = PLUGIN_UI_OUTSIDE;
    dataContext.tokenContext.pluginUiCurrentItem = 0;
    ux_flow_init(0, ux_plugin_approval_flow, NULL);
}
