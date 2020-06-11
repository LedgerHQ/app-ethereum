#include "shared_context.h"
#include "ui_callbacks.h"

UX_FLOW_DEF_NOCB(ux_approval_allowance_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(
    ux_approval_allowance_2_step,
    bnnn_paging,
    {
      .title = "Allowance",
      .text = " "
    });

UX_FLOW_DEF_NOCB(
    ux_approval_allowance_3_step,
    bnnn_paging,
    {
      .title = "Contract Name",
      .text = strings.common.fullAddress,
    });

UX_FLOW_DEF_NOCB(
    ux_approval_allowance_4_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = strings.common.fullAmount
    });

UX_FLOW_DEF_NOCB(
    ux_approval_allowance_5_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_FLOW_DEF_VALID(
    ux_approval_allowance_6_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_FLOW_DEF_VALID(
    ux_approval_allowance_7_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_approval_allowance_flow [] = {
  &ux_approval_allowance_1_step,
  &ux_approval_allowance_2_step,
  &ux_approval_allowance_3_step,
  &ux_approval_allowance_4_step,
  &ux_approval_allowance_5_step,
  &ux_approval_allowance_6_step,
  &ux_approval_allowance_7_step,
  FLOW_END_STEP,
};

