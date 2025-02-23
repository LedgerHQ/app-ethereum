#include "ui_callbacks.h"

// clang-format off
UX_STEP_NOCB(ux_auth7702_review_step_1,
    pnn,
    {
      &C_icon_warning,
      "You are authorizing",
      "the upgrade of your",
    });
UX_STEP_NOCB(ux_auth7702_review_step_2,
    pnn,
    {
      &C_icon_warning,
      "account into a",
      "smart account",
    });
UX_STEP_NOCB(ux_auth7702_account_step,
    bnnn_paging,
    {
      .title = "Account",
      .text = strings.common.fromAddress
    });
UX_STEP_NOCB(ux_auth7702_delegate_step,
    bnnn_paging,
    {
      .title = "Delegate",
      .text = strings.common.toAddress
    });
UX_STEP_NOCB(ux_auth7702_nonce_step,
    bnnn_paging,
    {
      .title = "Nonce",
      .text = strings.common.nonce
    });
UX_STEP_NOCB(ux_auth7702_network_step,
    bnnn_paging,
    {
      .title = "Network",
      .text = strings.common.network_name
    });
UX_STEP_CB(
    ux_auth7702_accept_step,
    pbb,
    auth_7702_ok_cb(),
    {
      &C_icon_validate_14,
      "Accept",
      "and sign",
    });
UX_STEP_CB(
    ux_auth7702_reject_step,
    pb,
    auth_7702_cancel_cb(),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_auth7702_flow,
	&ux_auth7702_review_step_1,
	&ux_auth7702_review_step_2,
	&ux_auth7702_account_step,
	&ux_auth7702_delegate_step,
	&ux_auth7702_nonce_step,
	&ux_auth7702_network_step,
	&ux_auth7702_accept_step,
	&ux_auth7702_reject_step);
