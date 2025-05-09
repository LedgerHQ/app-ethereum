#ifdef HAVE_BAGL

#include "common_ui.h"
#include "ux.h"
#include "ui_flow.h"
#include "ui_callbacks.h"

void ui_idle(void) {
    // reserve a display stack slot if none yet
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

void ui_error_blind_signing(void) {
    ux_flow_init(0, ux_error_blind_signing_flow, NULL);
}

void ui_warning_blind_signing(void) {
    ux_flow_init(0, ux_warning_blind_signing_flow, NULL);
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

unsigned int address_cancel_cb(void) {
    ui_idle();
    return io_seproxyhal_touch_address_cancel();
}

unsigned int tx_ok_cb(void) {
    ui_idle();
    return io_seproxyhal_touch_tx_ok();
}

unsigned int tx_cancel_cb(void) {
    ui_idle();
    return io_seproxyhal_touch_tx_cancel();
}

#ifdef HAVE_EIP7702

void ui_sign_7702_auth(void) {
    ux_flow_init(0, ux_auth7702_flow, NULL);
}

void ui_sign_7702_revocation(void) {
    ux_flow_init(0, ux_revocation7702_flow, NULL);
}

void ui_error_no_7702(void) {
    ux_flow_init(0, ux_error_7702_not_enabled_flow, NULL);
}

#ifdef HAVE_EIP7702_WHITELIST
void ui_error_no_7702_whitelist(void) {
    ux_flow_init(0, ux_error_7702_not_whitelisted_flow, NULL);
}
#endif  // HAVE_EIP7702_WHITELIST

#endif  // HAVE_EIP7702

#endif  // HAVE_BAGL
