#include "shared_context.h"
#include "ui_callbacks.h"

void prepare_domain_hash_v0() {
   snprintf(strings.tmp.tmp, 70, "0x%.*H", 32, tmpCtx.messageSigningContext712.domainHash);  
}

void prepare_message_hash_v0() {
   snprintf(strings.tmp.tmp, 70, "0x%.*H", 32, tmpCtx.messageSigningContext712.messageHash);  
}

UX_FLOW_DEF_NOCB(
    ux_sign_712_v0_flow_1_step,
    pnn,
    {
      &C_icon_certificate,
      "Sign",
      "typed message",
    });
UX_STEP_NOCB_INIT(
    ux_sign_712_v0_flow_2_step,
    bnnn_paging,
    prepare_domain_hash_v0(),
    {
      .title = "Domain hash",
      .text = strings.tmp.tmp,
    });
UX_STEP_NOCB_INIT(
    ux_sign_712_v0_flow_3_step,
    bnnn_paging,
    prepare_message_hash_v0(),
    {
      .title = "Message hash",
      .text = strings.tmp.tmp,
    });
UX_FLOW_DEF_VALID(
    ux_sign_712_v0_flow_4_step,
    pbb,
    io_seproxyhal_touch_signMessage712_v0_ok(NULL),
    {
      &C_icon_validate_14,
      "Sign",
      "message",
    });
UX_FLOW_DEF_VALID(
    ux_sign_712_v0_flow_5_step,
    pbb,
    io_seproxyhal_touch_signMessage712_v0_cancel(NULL),
    {
      &C_icon_crossmark,
      "Cancel",
      "signature",
    });

const ux_flow_step_t *        const ux_sign_712_v0_flow [] = {
  &ux_sign_712_v0_flow_1_step,
  &ux_sign_712_v0_flow_2_step,
  &ux_sign_712_v0_flow_3_step,
  &ux_sign_712_v0_flow_4_step,
  &ux_sign_712_v0_flow_5_step,
  FLOW_END_STEP,
};

