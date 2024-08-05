#include "shared_context.h"
#include "ui_callbacks.h"

// clang-format off
UX_STEP_NOCB(
    ux_display_public_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Verify",
      "address",
    });
UX_STEP_NOCB(
    ux_display_public_flow_2_step,
    bnnn_paging,
    {
      .title = "Address",
      .text = strings.common.toAddress,
    });
UX_STEP_CB(
    ux_display_public_flow_3_step,
    pb,
    io_seproxyhal_touch_address_ok(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_display_public_flow_4_step,
    pb,
    io_seproxyhal_touch_address_cancel(),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_display_public_flow,
        &ux_display_public_flow_1_step,
        &ux_display_public_flow_2_step,
        &ux_display_public_flow_3_step,
        &ux_display_public_flow_4_step);
