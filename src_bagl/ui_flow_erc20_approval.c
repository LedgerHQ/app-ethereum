#include "shared_context.h"
#include "ui_callbacks.h"

// clang-format off
UX_STEP_NOCB(ux_approval_allowance_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_STEP_NOCB(
    ux_approval_allowance_2_step,
    bnnn_paging,
    {
      .title = "Allowance",
      .text = " "
    });

UX_STEP_NOCB(
    ux_approval_allowance_3_step,
    bnnn_paging,
    {
      .title = "Contract Name",
      .text = strings.common.toAddress,
    });

UX_STEP_NOCB(
    ux_approval_allowance_4_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = strings.common.fullAmount
    });

UX_STEP_NOCB(
    ux_approval_allowance_5_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_STEP_CB(
    ux_approval_allowance_6_step,
    pbb,
    io_seproxyhal_touch_tx_ok(),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_STEP_CB(
    ux_approval_allowance_7_step,
    pb,
    io_seproxyhal_touch_tx_cancel(),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_approval_allowance_flow,
        &ux_approval_allowance_1_step,
        &ux_approval_allowance_2_step,
        &ux_approval_allowance_3_step,
        &ux_approval_allowance_4_step,
        &ux_approval_allowance_5_step,
        &ux_approval_allowance_6_step,
        &ux_approval_allowance_7_step);
