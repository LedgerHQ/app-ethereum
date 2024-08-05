#include "shared_context.h"
#include "ui_callbacks.h"

// clang-format off
UX_STEP_NOCB(
    ux_display_privacy_public_key_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Provide public",
      "privacy key",
    });
UX_STEP_NOCB(
    ux_display_privacy_public_key_flow_2_step,
    bnnn_paging,
    {
      .title = "Address",
      .text = strings.common.toAddress,
    });
UX_STEP_NOCB(
    ux_display_privacy_public_key_flow_3_step,
    bnnn_paging,
    {
      .title = "Key",
      .text = strings.common.fullAmount,
    });
UX_STEP_CB(
    ux_display_privacy_public_key_flow_4_step,
    pb,
    io_seproxyhal_touch_privacy_ok(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_display_privacy_public_key_flow_5_step,
    pb,
    io_seproxyhal_touch_privacy_cancel(),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_STEP_NOCB(
    ux_display_privacy_shared_secret_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Provide privacy",
      "secret key",
    });

// clang-format on

UX_FLOW(ux_display_privacy_public_key_flow,
        &ux_display_privacy_public_key_flow_1_step,
        &ux_display_privacy_public_key_flow_2_step,
        &ux_display_privacy_public_key_flow_3_step,
        &ux_display_privacy_public_key_flow_4_step,
        &ux_display_privacy_public_key_flow_5_step);

UX_FLOW(ux_display_privacy_shared_secret_flow,
        &ux_display_privacy_shared_secret_flow_1_step,
        &ux_display_privacy_public_key_flow_2_step,
        &ux_display_privacy_public_key_flow_3_step,
        &ux_display_privacy_public_key_flow_4_step,
        &ux_display_privacy_public_key_flow_5_step);
