#include "shared_context.h"
#include "ui_callbacks.h"
#include "common_ui.h"
#include "sign_message.h"

typedef enum { UI_191_POS_REVIEW, UI_191_POS_QUESTION, UI_191_POS_END } e_ui_191_position;

static uint8_t ui_pos;

static void dummy_pre_cb(void) {
    if (ui_pos == UI_191_POS_REVIEW) {
        question_switcher();
    } else {
        ux_flow_prev();
        ui_pos = UI_191_POS_REVIEW;
    }
}

static void dummy_post_cb(void) {
    if (ui_pos == UI_191_POS_QUESTION) {
        // temporarily disable button clicks, they will be re-enabled as soon as new data
        // is received and the page is redrawn with ux_flow_init()
        G_ux.stack[0].button_push_callback = NULL;
        continue_displaying_message();
    } else  // UI_191_END
    {
        ui_191_switch_to_message_end();
    }
}

static unsigned int signMessage_ok_cb(void) {
    ui_idle();
    return io_seproxyhal_touch_signMessage_ok();
}

static unsigned int signMessage_cancel_cb(void) {
    ui_idle();
    return io_seproxyhal_touch_signMessage_cancel();
}

// clang-format off
UX_STEP_NOCB(
    ux_191_step_review,
    pnn,
    {
      &C_icon_certificate,
      "Review",
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
#ifdef TARGET_NANOS
    nn,
#else
    nnn,
#endif
    G_ux.stack[0].button_push_callback = NULL; // disable button clicks
    skip_rest_of_message(),
    {
#ifndef TARGET_NANOS
      "Press right to",
      "continue message",
#else
      "Press right to read",
#endif
      "Double-press to skip"
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
    signMessage_ok_cb(),
    {
      &C_icon_validate_14,
      "Sign",
      "message",
    });
UX_STEP_CB(
    ux_191_step_cancel,
    pbb,
    signMessage_cancel_cb(),
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
        &ux_191_step_theres_more,
        &ux_191_step_dummy_post,
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

void ui_191_switch_to_message_end(void) {
    // Force it to a value that will make it automatically do a prev()
    ui_pos = UI_191_POS_QUESTION;
    ux_flow_init(0, ux_191_flow, &ux_191_step_dummy_pre);
}

void ui_191_switch_to_sign(void) {
    ux_flow_init(0, ux_191_flow, &ux_191_step_sign);
    ui_pos = UI_191_POS_END;
}

void ui_191_switch_to_question(void) {
    ux_flow_init(0, ux_191_flow, &ux_191_step_theres_more);
    ui_pos = UI_191_POS_QUESTION;
}
