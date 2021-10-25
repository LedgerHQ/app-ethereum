#ifdef HAVE_ETH2

#include "shared_context.h"
#include "ui_callbacks.h"

void prepare_eth2_public_key() {
    snprintf(strings.tmp.tmp, 100, "0x%.*H", 48, tmpCtx.publicKeyContext.publicKey.W);
}

// clang-format off
UX_STEP_NOCB(
    ux_display_public_eth2_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Verify ETH2",
      "public key",
    });
UX_STEP_NOCB_INIT(
    ux_display_public_eth2_flow_2_step,
    bnnn_paging,
    prepare_eth2_public_key(),
    {
      .title = "Public Key",
      .text = strings.tmp.tmp,
    });
UX_STEP_CB(
    ux_display_public_eth2_flow_3_step,
    pb,
    io_seproxyhal_touch_eth2_address_ok(NULL),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_display_public_eth2_flow_4_step,
    pb,
    io_seproxyhal_touch_address_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_display_public_eth2_flow,
        &ux_display_public_eth2_flow_1_step,
        &ux_display_public_eth2_flow_2_step,
        &ux_display_public_eth2_flow_3_step,
        &ux_display_public_eth2_flow_4_step);

#endif
