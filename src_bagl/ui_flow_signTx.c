#include "shared_context.h"
#include "ui_callbacks.h"
#include "chainConfig.h"
#include "common_utils.h"
#include "feature_signTx.h"
#include "network.h"
#include "eth_plugin_handler.h"
#include "ui_plugin.h"
#include "common_ui.h"
#include "plugins.h"
#include "domain_name.h"
#include "ui_domain_name.h"

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
    io_seproxyhal_touch_data_ok(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_confirm_selector_flow_4_step,
    pb,
    io_seproxyhal_touch_data_cancel(),
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
    io_seproxyhal_touch_data_ok(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_confirm_parameter_flow_4_step,
    pb,
    io_seproxyhal_touch_data_cancel(),
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
    ux_approval_from_step,
    bnnn_paging,
    {
      .title = "From",
      .text = strings.common.fromAddress,
    });
UX_STEP_NOCB(
    ux_approval_to_step,
    bnnn_paging,
    {
      .title = "To",
      .text = strings.common.toAddress,
    });

UX_STEP_NOCB_INIT(
  ux_plugin_approval_id_step,
  bnnn_paging,
  plugin_ui_get_id(),
  {
    .title = strings.common.toAddress,
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
    .title = strings.common.toAddress,
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
    ux_approval_fees_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_STEP_NOCB(
    ux_approval_network_step,
    bnnn_paging,
    {
      .title = "Network",
      .text = strings.common.network_name,
    });

UX_STEP_CB(
    ux_approval_accept_step,
    pbb,
    io_seproxyhal_touch_tx_ok(),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_approval_reject_step,
    pb,
    io_seproxyhal_touch_tx_cancel(),
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

UX_STEP_NOCB(ux_approval_blind_signing_reminder_step,
    pbb,
    {
      &C_icon_warning,
      "You accepted",
      "the risks",
    });
// clang-format on

const ux_flow_step_t *ux_approval_tx_flow[15];

void ux_approve_tx(bool fromPlugin) {
    int step = 0;
    ux_approval_tx_flow[step++] = &ux_approval_review_step;

    if (fromPlugin) {
        // Add the special dynamic display logic
        ux_approval_tx_flow[step++] = &ux_plugin_approval_id_step;
        if (pluginType != EXTERNAL) {
            if (strings.common.fromAddress[0] != 0) {
                ux_approval_tx_flow[step++] = &ux_approval_from_step;
            }
        }
        ux_approval_tx_flow[step++] = &ux_plugin_approval_before_step;
        ux_approval_tx_flow[step++] = &ux_plugin_approval_display_step;
        ux_approval_tx_flow[step++] = &ux_plugin_approval_after_step;
    } else {
        // We're in a regular transaction, just show the amount and the address
        if (strings.common.fromAddress[0] != 0) {
            ux_approval_tx_flow[step++] = &ux_approval_from_step;
        }
        ux_approval_tx_flow[step++] = &ux_approval_amount_step;
#ifdef HAVE_DOMAIN_NAME
        uint64_t chain_id = get_tx_chain_id();
        if (has_domain_name(&chain_id, tmpContent.txContent.destination)) {
            ux_approval_tx_flow[step++] = &ux_domain_name_step;
            if (N_storage.verbose_domain_name) {
                ux_approval_tx_flow[step++] = &ux_approval_to_step;
            }
        } else {
#endif  // HAVE_DOMAIN_NAME
            ux_approval_tx_flow[step++] = &ux_approval_to_step;
#ifdef HAVE_DOMAIN_NAME
        }
#endif  // HAVE_DOMAIN_NAME
    }

    if (N_storage.displayNonce) {
        ux_approval_tx_flow[step++] = &ux_approval_nonce_step;
    }

    uint64_t chain_id = get_tx_chain_id();
    if ((chainConfig->chainId == ETHEREUM_MAINNET_CHAINID) && (chain_id != chainConfig->chainId)) {
        ux_approval_tx_flow[step++] = &ux_approval_network_step;
    }

    ux_approval_tx_flow[step++] = &ux_approval_fees_step;
    if (!fromPlugin && tmpContent.txContent.dataPresent && !N_storage.contractDetails) {
        ux_approval_tx_flow[step++] = &ux_approval_blind_signing_reminder_step;
    }
    ux_approval_tx_flow[step++] = &ux_approval_accept_step;
    ux_approval_tx_flow[step++] = &ux_approval_reject_step;
    ux_approval_tx_flow[step++] = FLOW_END_STEP;

    ux_flow_init(0, ux_approval_tx_flow, NULL);
}
