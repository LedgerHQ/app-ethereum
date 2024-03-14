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

void ui_display_public_key(const uint64_t *chain_id) {
    (void) chain_id;
    ux_flow_init(0, ux_display_public_flow, NULL);
}

void ui_sign_712_v0(void) {
    ux_flow_init(0, ux_sign_712_v0_flow, NULL);
}

void ui_confirm_selector(void) {
    ux_flow_init(0, ux_confirm_selector_flow, NULL);
}

void ui_confirm_parameter(void) {
    ux_flow_init(0, ux_confirm_parameter_flow, NULL);
}

#endif  // HAVE_BAGL
