#ifdef HAVE_EIP712_FULL_SUPPORT

#include "ui_logic.h"
#include "shared_context.h"  // strings
#include "common_ui.h"
#include "ui_flow.h"  // ux_warning_blind_signing_warn_step

enum { UI_712_POS_REVIEW, UI_712_POS_END };
static uint8_t ui_pos;
static bool filtered;

// forward declaration for the BAGL step
static void ui_712_start_flow(void);

static void dummy_cb(void) {
    switch (ui_712_next_field()) {
        case EIP712_NO_MORE_FIELD:
            if (ui_pos == UI_712_POS_REVIEW) {
                ux_flow_next();
                ui_pos = UI_712_POS_END;
            } else {
                // Keep user at the end of the flow
                ux_flow_next();
            }
            break;
        case EIP712_FIELD_INCOMING:
            // temporarily disable button clicks, they will be re-enabled as soon as new data
            // is received and the page is redrawn with ux_flow_init()
            G_ux.stack[0].button_push_callback = NULL;
            break;
        case EIP712_FIELD_LATER:
        default:
            break;
    }
}

static unsigned int _approve_cb(void) {
    ui_idle();
    return ui_712_approve();
}

static unsigned int _reject_cb(void) {
    ui_idle();
    return ui_712_reject();
}

// clang-format off
UX_STEP_NOCB(
    ux_712_step_review,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "typed message",
    });
UX_STEP_NOCB(
   ux_712_step_dynamic,
   bnnn_paging,
   {
      .title = strings.tmp.tmp2,
      .text = strings.tmp.tmp,
   }
);
UX_STEP_INIT(
   ux_712_step_dummy,
   NULL,
   NULL,
   {
    dummy_cb();
   }
);
UX_STEP_CB(
    ux_712_step_approve,
    pb,
    _approve_cb(),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_712_step_approve_risky,
    pbb,
    _approve_cb(),
    {
      &C_icon_validate_14,
      "Accept risk",
      "and sign",
    });
UX_STEP_CB(
    ux_712_step_reject,
    pb,
    _reject_cb(),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_STEP_INIT(
   ux_712_warning_blind_signing_jump_step,
   NULL,
   NULL,
   {
     ui_712_start_flow();
   }
);
// clang-format on

UX_FLOW(ux_712_flow,
        &ux_712_step_review,
        &ux_712_step_dynamic,
        &ux_712_step_dummy,
        &ux_712_step_approve,
        &ux_712_step_reject);

UX_FLOW(ux_712_flow_unfiltered,
        &ux_712_step_review,
        &ux_712_step_dynamic,
        &ux_712_step_dummy,
        &ux_712_step_approve_risky,
        &ux_712_step_reject);

UX_FLOW(ux_712_warning_blind_signing_flow,
        &ux_warning_blind_signing_warn_step,
        &ux_712_warning_blind_signing_jump_step);

static void ui_712_start_flow(void) {
    ux_flow_init(0, filtered ? ux_712_flow : ux_712_flow_unfiltered, NULL);
    ui_pos = UI_712_POS_REVIEW;
}
void ui_712_start(void) {
    filtered = true;
    ui_712_start_flow();
}

void ui_712_start_unfiltered(void) {
    filtered = false;
    ux_flow_init(0, ux_712_warning_blind_signing_flow, NULL);
}

void ui_712_switch_to_message(void) {
    ux_flow_init(0, filtered ? ux_712_flow : ux_712_flow_unfiltered, &ux_712_step_dynamic);
    ui_pos = UI_712_POS_REVIEW;
}

void ui_712_switch_to_sign(void) {
    ux_flow_init(0,
                 filtered ? ux_712_flow : ux_712_flow_unfiltered,
                 filtered ? &ux_712_step_approve : &ux_712_step_approve_risky);
    ui_pos = UI_712_POS_END;
}

#endif  // HAVE_EIP712_FULL_SUPPORT
