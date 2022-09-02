#include "shared_context.h"
#include "ui_callbacks.h"
#include "sign_message.h"


// clang-format off
UX_STEP_NOCB(
    ux_191_step_review,
    pnn,
    {
      &C_icon_certificate,
      "Sign",
      "message",
    });
UX_STEP_NOCB(
    ux_191_step_message,
    bnnn_paging,
    {
      .title = "Message",
      .text = strings.tmp.tmp,
    });
UX_STEP_INIT(
    ux_191_step_dummy_pre,
    NULL,
    NULL,
    {
      dummy_pre_cb();
    });
UX_STEP_CB(
    ux_191_step_theres_more,
    bn,
    theres_more_click_cb(),
    {
      "There's more!",
      "Click to skip"
    });
UX_STEP_INIT(
    ux_191_step_dummy_post,
    NULL,
    NULL,
    {
      dummy_post_cb();
    });
UX_STEP_CB(
    ux_191_step_sign,
    pbb,
    io_seproxyhal_touch_signMessage_ok(),
    {
      &C_icon_validate_14,
      "Sign",
      "message",
    });
UX_STEP_CB(
    ux_191_step_cancel,
    pbb,
    io_seproxyhal_touch_signMessage_cancel(),
    {
      &C_icon_crossmark,
      "Cancel",
      "signature",
    });
// clang-format on

UX_FLOW(ux_sign_flow,
        &ux_191_step_review,
        &ux_191_step_message,
        &ux_191_step_dummy_pre,
        &ux_191_step_theres_more,
        &ux_191_step_dummy_post,
        &ux_191_step_sign,
        &ux_191_step_cancel);


void    ui_191_switch_to_message(void)
{
    ux_flow_init(0, ux_sign_flow, &ux_191_step_message);
}

void    ui_191_switch_to_message_end(void)
{
    ux_flow_init(0, ux_sign_flow, &ux_191_step_dummy_pre);
    // with pos != REVIEW, automatically goes back to previous step
}

void    ui_191_switch_to_sign(void)
{
    ux_flow_init(0, ux_sign_flow, &ux_191_step_sign);
}

void    ui_191_switch_to_question(void)
{
    ux_flow_init(0, ux_sign_flow, &ux_191_step_theres_more);
}
