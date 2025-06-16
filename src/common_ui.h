#pragma once

#include <stdint.h>
#include <stdbool.h>

void ui_idle(void);
void ui_error_blind_signing(void);
void ui_display_public_eth2(void);
void ui_display_privacy_public_key(void);
void ui_display_privacy_shared_secret(void);
void ui_display_public_key(const uint64_t *chain_id);
void ui_sign_712_v0(void);
void ui_confirm_selector(void);
void ui_confirm_parameter(void);

// EIP-191
void ui_191_start(const char *message);

// EIP-712
void ui_712_start(void);
void ui_712_start_unfiltered(void);
void ui_712_switch_to_message(void);
void ui_712_switch_to_sign(void);

// Generic clear-signing
bool ui_gcs(void);
void ui_gcs_cleanup(void);

// EIP-7702
void ui_sign_7702_auth(void);
void ui_sign_7702_revocation(void);
void ui_error_no_7702(void);
void ui_error_no_7702_whitelist(void);
