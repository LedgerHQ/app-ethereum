#include "shared_context.h"
#include "ui_callbacks.h"
#include "ui_flow_signMessage.h"
#include "sign_message.h"

static uint8_t ui_pos;

static void dummy_pre_cb(void) {
    if (ui_pos == UI_191_POS_REVIEW) {
#ifdef TARGET_NANOS
        skip_rest_of_message();
#else
        question_switcher();
#endif
    } else {
        ux_flow_prev();
        ui_pos = UI_191_POS_REVIEW;
    }
}

#ifndef TARGET_NANOS
static void dummy_post_cb(void) {
    if (ui_pos == UI_191_POS_QUESTION) {
        continue_displaying_message();
    } else  // UI_191_END
    {
        ui_191_switch_to_message_end();
    }
}
#endif

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
#ifndef TARGET_NANOS
UX_STEP_CB(
    ux_191_step_theres_more,
    nnn,
    skip_rest_of_message(),
    {
      "Press right to",
      "continue message",
      "Double-press to skip"
    });
UX_STEP_INIT(
    ux_191_step_dummy_post,
    NULL,
    NULL,
    {
      dummy_post_cb();
    });
#endif
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

UX_FLOW(ux_191_flow,
        &ux_191_step_review,
        &ux_191_step_message,
        &ux_191_step_dummy_pre,
#ifndef TARGET_NANOS
        &ux_191_step_theres_more,
        &ux_191_step_dummy_post,
#endif
        &ux_191_step_sign,
        &ux_191_step_cancel);

void ui_191_start(void) {
    ux_flow_init(0, ux_191_flow, NULL);
    ui_pos = UI_191_POS_REVIEW;
}

void ui_191_switch_to_message(void) {
    ux_flow_init(0, ux_191_flow, &ux_191_step_message);
    ui_pos = UI_191_POS_REVIEW;
}

#ifndef TARGET_NANOS
void ui_191_switch_to_message_end(void) {
    // Force it to a value that will make it automatically do a prev()
    ui_pos = UI_191_POS_QUESTION;
    ux_flow_init(0, ux_191_flow, &ux_191_step_dummy_pre);
}
#endif

void ui_191_switch_to_sign(void) {
    ux_flow_init(0, ux_191_flow, &ux_191_step_sign);
    ui_pos = UI_191_POS_END;
}

#ifndef TARGET_NANOS
void ui_191_switch_to_question(void) {
    ux_flow_init(0, ux_191_flow, &ux_191_step_theres_more);
    ui_pos = UI_191_POS_QUESTION;
}
#endif
