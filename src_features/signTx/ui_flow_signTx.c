#include "shared_context.h"
#include "ui_callbacks.h"
#include "chainConfig.h"
#include "utils.h"

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

const ux_flow_step_t *ux_approval_tx_flow[12];

void ux_approve_tx(bool dataPresent) {
    int step = 0;
    ux_approval_tx_flow[step++] = &ux_approval_review_step;
    if (tmpContent.txContent.dataPresent && !N_storage.contractDetails) {
        ux_approval_tx_flow[step++] = &ux_approval_data_warning_step;
    }
    ux_approval_tx_flow[step++] = &ux_approval_amount_step;
    ux_approval_tx_flow[step++] = &ux_approval_address_step;
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