#ifdef HAVE_ETH2

#include "shared_context.h"
#include "ui_callbacks.h"
#include "uint_common.h"

void prepare_eth2_public_key() {
    array_bytes_string(strings.tmp.tmp,
                       sizeof(strings.tmp.tmp),
                       tmpCtx.publicKeyContext.publicKey.W,
                       48);
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
    io_seproxyhal_touch_eth2_address_ok(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_display_public_eth2_flow_4_step,
    pb,
    address_cancel_cb(),
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
