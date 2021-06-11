#include "shared_context.h"
#include "ui_callbacks.h"
#include "chainConfig.h"
#include "utils.h"
#include "feature_signTx.h"
#include "eth_plugin_handler.h"

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

// clang-format off
UX_STEP_NOCB(
    ux_confirm_selector_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Verify",
      "selector",
    });

UX_STEP_NOCB(
    ux_confirm_selector_flow_2_step,
    bn,
    {
      "Selector",
      strings.tmp.tmp
    });
UX_STEP_CB(
    ux_confirm_selector_flow_3_step,
    pb,
    io_seproxyhal_touch_data_ok(NULL),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_confirm_selector_flow_4_step,
    pb,
    io_seproxyhal_touch_data_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_confirm_selector_flow,
        &ux_confirm_selector_flow_1_step,
        &ux_confirm_selector_flow_2_step,
        &ux_confirm_selector_flow_3_step,
        &ux_confirm_selector_flow_4_step);

//////////////////////////////////////////////////////////////////////
// clang-format off
UX_STEP_NOCB(
    ux_confirm_parameter_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Verify",
      strings.tmp.tmp2
    });
UX_STEP_NOCB(
    ux_confirm_parameter_flow_2_step,
    bnnn_paging,
    {
      .title = "Parameter",
      .text = strings.tmp.tmp,
    });
UX_STEP_CB(
    ux_confirm_parameter_flow_3_step,
    pb,
    io_seproxyhal_touch_data_ok(NULL),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_confirm_parameter_flow_4_step,
    pb,
    io_seproxyhal_touch_data_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_confirm_parameter_flow,
        &ux_confirm_parameter_flow_1_step,
        &ux_confirm_parameter_flow_2_step,
        &ux_confirm_parameter_flow_3_step,
        &ux_confirm_parameter_flow_4_step);

//////////////////////////////////////////////////////////////////////
// clang-format off
UX_STEP_NOCB(ux_approval_review_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });
UX_STEP_NOCB(
    ux_approval_amount_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = strings.common.fullAmount
    });
UX_STEP_NOCB(
    ux_approval_address_step,
    bnnn_paging,
    {
      .title = "Address",
      .text = strings.common.fullAddress,
    });

UX_STEP_NOCB_INIT(
  ux_plugin_approval_id_step,
  bnnn_paging,
  plugin_ui_get_id(),
  {
    .title = strings.common.fullAddress,
    .text = strings.common.fullAmount
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
    .title = strings.common.fullAddress,
    .text = strings.common.fullAmount
  });

UX_STEP_INIT(
  ux_plugin_approval_after_step,
  NULL,
  NULL,
  {
    display_next_plugin_item(false);
  });

UX_STEP_NOCB(
    ux_approval_base_fee_step,
    bnnn_paging,
    {
      .title = "Base Fee",
      .text = strings.common.maxFee,
    });
UX_STEP_NOCB(
    ux_approval_priority_fee_step,
    bnnn_paging,
    {
      .title = "Priority Fee",
      .text = strings.common.priorityFee,
    });
UX_STEP_NOCB(
    ux_approval_fees_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });
UX_STEP_NOCB(
    ux_approval_chainid_step,
    bnnn_paging,
    {
      .title = "Chain ID",
      .text = strings.common.chainID,
    });

UX_STEP_CB(
    ux_approval_accept_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_approval_reject_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_STEP_NOCB(
    ux_approval_nonce_step,
    bnnn_paging,
    {
      .title = "Nonce",
      .text = strings.common.nonce,
    });

UX_STEP_NOCB(ux_approval_data_warning_step,
    pbb,
    {
      &C_icon_warning,
      "Data",
      "Present",
    });
// clang-format on

const ux_flow_step_t *ux_approval_tx_flow[15];

void ux_approve_tx(bool fromPlugin) {
    int step = 0;
    ux_approval_tx_flow[step++] = &ux_approval_review_step;

    if (tmpContent.txContent.dataPresent && !N_storage.contractDetails) {
        ux_approval_tx_flow[step++] = &ux_approval_data_warning_step;
    }

    if (fromPlugin) {
        prepareChainIdDisplay();
        prepareFeeDisplay();
        ux_approval_tx_flow[step++] = &ux_plugin_approval_id_step;
        ux_approval_tx_flow[step++] = &ux_plugin_approval_before_step;
        ux_approval_tx_flow[step++] = &ux_plugin_approval_display_step;
        ux_approval_tx_flow[step++] = &ux_plugin_approval_after_step;
    } else {
        ux_approval_tx_flow[step++] = &ux_approval_amount_step;
        ux_approval_tx_flow[step++] = &ux_approval_address_step;
    }

    if (N_storage.displayNonce) {
        ux_approval_tx_flow[step++] = &ux_approval_nonce_step;
    }

    uint32_t id;
    if (txContext.txType == LEGACY) {
        id = u32_from_BE(txContext.content->v, txContext.content->vLength, true);
    } else if (txContext.txType == EIP2930 || txContext.txType == EIP1559) {
        id =
            u32_from_BE(txContext.content->chainID.value, txContext.content->chainID.length, false);
    } else {
        PRINTF("TxType `%u` not supported while preparing to approve tx\n", txContext.txType);
        THROW(0x6501);
    }
    if (id != ETHEREUM_MAINNET_CHAINID) {
        ux_approval_tx_flow[step++] = &ux_approval_chainid_step;
    }
    if (txContext.txType == EIP1559 && N_storage.displayFeeDetails) {
      ux_approval_tx_flow[step++] = &ux_approval_base_fee_step;
      ux_approval_tx_flow[step++] = &ux_approval_priority_fee_step;
    } else {
      ux_approval_tx_flow[step++] = &ux_approval_fees_step;
    }
    ux_approval_tx_flow[step++] = &ux_approval_accept_step;
    ux_approval_tx_flow[step++] = &ux_approval_reject_step;
    ux_approval_tx_flow[step++] = FLOW_END_STEP;

    ux_flow_init(0, ux_approval_tx_flow, NULL);
}