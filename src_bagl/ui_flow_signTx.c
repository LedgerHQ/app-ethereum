#include "shared_context.h"
#include "ui_callbacks.h"
#include "chainConfig.h"
#include "common_utils.h"
#include "network.h"
#include "eth_plugin_handler.h"
#include "ui_plugin.h"
#include "common_ui.h"
#include "plugins.h"
#include "trusted_name.h"
#include "ui_trusted_name.h"

static unsigned int data_ok_cb(void) {
    ui_idle();
    return io_seproxyhal_touch_data_ok();
}

static unsigned int data_cancel_cb(void) {
    ui_idle();
    return io_seproxyhal_touch_data_cancel();
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
    data_ok_cb(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_confirm_selector_flow_4_step,
    pb,
    data_cancel_cb(),
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
    data_ok_cb(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_confirm_parameter_flow_4_step,
    pb,
    data_cancel_cb(),
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
UX_STEP_NOCB(ux_approval_tx_hash_step,
    bnnn_paging,
    {
#ifdef TARGET_NANOS
      .title = "TX hash",
#else
      .title = "Transaction hash",
#endif
      .text = strings.common.tx_hash
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
    tx_ok_cb(),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_approval_accept_blind_step,
    pbb,
    tx_ok_cb(),
    {
      &C_icon_validate_14,
      "Accept risk",
      "and send",
    });
UX_STEP_CB(
    ux_approval_reject_step,
    pb,
    tx_cancel_cb(),
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
        if (tmpContent.txContent.dataPresent) {
#pragma GCC diagnostic ignored "-Wformat"
            snprintf(strings.common.tx_hash,
                     sizeof(strings.common.tx_hash),
                     "0x%.*h",
                     sizeof(tmpCtx.transactionContext.hash),
                     tmpCtx.transactionContext.hash);
#pragma GCC diagnostic warning "-Wformat"
            ux_approval_tx_flow[step++] = &ux_approval_tx_hash_step;
        }
        // We're in a regular transaction, just show the amount and the address
        if (strings.common.fromAddress[0] != 0) {
            ux_approval_tx_flow[step++] = &ux_approval_from_step;
        }
        if (!tmpContent.txContent.dataPresent ||
            !allzeroes(tmpContent.txContent.value.value, tmpContent.txContent.value.length)) {
            ux_approval_tx_flow[step++] = &ux_approval_amount_step;
        }
#ifdef HAVE_TRUSTED_NAME
        uint64_t chain_id = get_tx_chain_id();
        e_name_type type = TN_TYPE_ACCOUNT;
        e_name_source source = TN_SOURCE_ENS;
        if (get_trusted_name(1, &type, 1, &source, &chain_id, tmpContent.txContent.destination) !=
            NULL) {
            ux_approval_tx_flow[step++] = &ux_trusted_name_step;
            if (N_storage.verbose_trusted_name) {
                ux_approval_tx_flow[step++] = &ux_approval_to_step;
            }
        } else {
#endif  // HAVE_TRUSTED_NAME
            ux_approval_tx_flow[step++] = &ux_approval_to_step;
#ifdef HAVE_TRUSTED_NAME
        }
#endif  // HAVE_TRUSTED_NAME
    }

    if (N_storage.displayNonce) {
        ux_approval_tx_flow[step++] = &ux_approval_nonce_step;
    }

    uint64_t chain_id = get_tx_chain_id();
    if ((chainConfig->chainId == ETHEREUM_MAINNET_CHAINID) && (chain_id != chainConfig->chainId)) {
        ux_approval_tx_flow[step++] = &ux_approval_network_step;
    }

    ux_approval_tx_flow[step++] = &ux_approval_fees_step;
    if (tmpContent.txContent.dataPresent) {
        ux_approval_tx_flow[step++] = &ux_approval_accept_blind_step;
    } else {
        ux_approval_tx_flow[step++] = &ux_approval_accept_step;
    }
    ux_approval_tx_flow[step++] = &ux_approval_reject_step;
    ux_approval_tx_flow[step++] = FLOW_END_STEP;

    ux_flow_init(0, ux_approval_tx_flow, NULL);
}
