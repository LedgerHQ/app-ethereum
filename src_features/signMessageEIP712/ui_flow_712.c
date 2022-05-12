#include "ui_flow_712.h"
#include "ui_logic.h"
#include "shared_context.h" // strings

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
      ui_712_next_field();
   }
);
UX_STEP_CB(
    ux_712_step_approve,
    pb,
    NULL,//io_seproxyhal_touch_signMessage712_ok(NULL),
    {
      &C_icon_validate_14,
      "Approve",
    });
UX_STEP_CB(
    ux_712_step_reject,
    pb,
    NULL,//io_seproxyhal_touch_signMessage712_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_712_flow,
        &ux_712_step_review,
        &ux_712_step_dynamic,
        &ux_712_step_dummy,
        &ux_712_step_approve,
        &ux_712_step_reject);
