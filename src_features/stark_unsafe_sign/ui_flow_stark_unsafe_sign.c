#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

unsigned int io_seproxyhal_touch_stark_unsafe_sign_ok(const bagl_element_t *e);

void stark_unsafe_sign_display_account() {
    snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "0x%.*H", 32, dataContext.starkContext.w1);
}

void stark_unsafe_sign_display_hash() {
    snprintf(strings.tmp.tmp, sizeof(strings.tmp.tmp), "0x%.*H", 32, dataContext.starkContext.w2);
}

// clang-format off
UX_STEP_NOCB(ux_stark_unsafe_sign_1_step,
    pnn,
    {
      &C_icon_warning,
      "Unsafe",
      "Stark Sign",
    });

UX_STEP_NOCB_INIT(
  ux_stark_unsafe_sign_2_step,
  bnnn_paging,
  stark_unsafe_sign_display_account(),
  {
    .title = "From Account",
    .text = strings.tmp.tmp
  });

UX_STEP_NOCB_INIT(
  ux_stark_unsafe_sign_3_step,
  bnnn_paging,
  stark_unsafe_sign_display_hash(),
  {
    .title = "Hash",
    .text = strings.tmp.tmp
  });


UX_STEP_CB(
    ux_stark_unsafe_sign_4_step,
    pbb,
    io_seproxyhal_touch_stark_unsafe_sign_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_stark_unsafe_sign_5_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });
// clang-format on

UX_FLOW(ux_stark_unsafe_sign_flow,
        &ux_stark_unsafe_sign_1_step,
        &ux_stark_unsafe_sign_2_step,
        &ux_stark_unsafe_sign_3_step,
        &ux_stark_unsafe_sign_4_step,
        &ux_stark_unsafe_sign_5_step);

#endif
