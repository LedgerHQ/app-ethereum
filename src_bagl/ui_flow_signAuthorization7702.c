#include "ui_callbacks.h"

// clang-format off
UX_STEP_NOCB(ux_7702_review_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "authorization",
    });
UX_STEP_NOCB(ux_7702_account_step,
    bnnn_paging,
    {
      .title = "Account",
      .text = strings.common.fromAddress
    });
UX_STEP_NOCB(ux_7702_delegate_step,
    bnnn_paging,
    {
      .title = "Delegate to",
      .text = strings.common.toAddress
    });
UX_STEP_NOCB(ux_7702_network_step,
    bnnn_paging,
    {
      .title = "Delegate on network",
      .text = strings.common.network_name
    });
UX_STEP_CB(
    ux_7702_accept_step,
    pbb,
    auth_7702_ok_cb(),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_7702_reject_step,
    pb,
    auth_7702_cancel_cb(),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format off

UX_FLOW(ux_auth7702_flow,
        &ux_7702_review_step,
        &ux_7702_account_step,
        &ux_7702_delegate_step,
        &ux_7702_network_step,
        &ux_7702_accept_step,
        &ux_7702_reject_step);

UX_FLOW(ux_revocation7702_flow,
        &ux_7702_review_step,
        &ux_7702_account_step,
        &ux_7702_network_step,
        &ux_7702_accept_step,
        &ux_7702_reject_step);
