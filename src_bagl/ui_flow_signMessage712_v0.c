#include "shared_context.h"
#include "ui_callbacks.h"
#include "common_712.h"
#include "uint_common.h"
#include "common_ui.h"
#include "ui_flow.h"  // ux_warning_blind_signing_warn_step

void prepare_domain_hash_v0() {
    array_bytes_string(strings.tmp.tmp,
                       sizeof(strings.tmp.tmp),
                       tmpCtx.messageSigningContext712.domainHash,
                       KECCAK256_HASH_BYTESIZE);
}

void prepare_message_hash_v0() {
    array_bytes_string(strings.tmp.tmp,
                       sizeof(strings.tmp.tmp),
                       tmpCtx.messageSigningContext712.messageHash,
                       KECCAK256_HASH_BYTESIZE);
}

static unsigned int _approve_cb(void) {
    ui_idle();
    return ui_712_approve_cb();
}

static unsigned int _reject_cb(void) {
    ui_idle();
    return ui_712_reject_cb();
}

// clang-format off
UX_STEP_NOCB(
    ux_sign_712_v0_flow_1_step,
    pnn,
    {
      &C_icon_certificate,
      "Review",
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
UX_STEP_CB(
    ux_sign_712_v0_flow_4_step,
    pbb,
    _approve_cb(),
    {
      &C_icon_validate_14,
      "Accept risk",
      "and sign",
    });
UX_STEP_CB(
    ux_sign_712_v0_flow_5_step,
    pbb,
    _reject_cb(),
    {
      &C_icon_crossmark,
      "Cancel",
      "signature",
    });
// clang-format on

UX_FLOW(ux_sign_712_v0_flow,
        &ux_warning_blind_signing_warn_step,
        &ux_sign_712_v0_flow_1_step,
        &ux_sign_712_v0_flow_2_step,
        &ux_sign_712_v0_flow_3_step,
        &ux_sign_712_v0_flow_4_step,
        &ux_sign_712_v0_flow_5_step);
