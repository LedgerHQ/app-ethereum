#ifdef HAVE_BAGL

#include "common_ui.h"
#include "ux.h"
#include "ui_flow.h"

void ui_idle(void) {
    // reserve a display stack slot if none yet
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

void ui_warning_contract_data(void) {
    ux_flow_init(0, ux_warning_contract_data_flow, NULL);
}

void ui_display_public_eth2(void) {
    ux_flow_init(0, ux_display_public_eth2_flow, NULL);
}

void ui_display_privacy_public_key(void) {
    ux_flow_init(0, ux_display_privacy_public_key_flow, NULL);
}

void ui_display_privacy_shared_secret(void) {
    ux_flow_init(0, ux_display_privacy_shared_secret_flow, NULL);
}

void ui_display_public_key(void) {
    ux_flow_init(0, ux_display_public_flow, NULL);
}

void ui_sign_712_v0(void) {
    ux_flow_init(0, ux_sign_712_v0_flow, NULL);
}

#ifdef HAVE_STARKWARE
void ui_display_stark_public(void) {
    ux_flow_init(0, ux_display_stark_public_flow, NULL);
}
void ui_stark_limit_order(void) {
    ux_flow_init(0, ux_stark_limit_order_flow, NULL);
}

void ui_stark_unsafe_sign(void) {
    ux_flow_init(0, ux_stark_unsafe_sign_flow, NULL);
}

void ui_stark_transfer(bool selfTransfer, bool conditional) {
    if (selfTransfer) {
        ux_flow_init(
            0,
            (conditional ? ux_stark_self_transfer_conditional_flow : ux_stark_self_transfer_flow),
            NULL);
    } else {
        ux_flow_init(0,
                     (conditional ? ux_stark_transfer_conditional_flow : ux_stark_transfer_flow),
                     NULL);
    }
}
#endif  // HAVE_STARKWARE

void ui_confirm_selector(void) {
    ux_flow_init(0, ux_confirm_selector_flow, NULL);
}

void ui_confirm_parameter(void) {
    ux_flow_init(0, ux_confirm_parameter_flow, NULL);
}

#endif  // HAVE_BAGL
