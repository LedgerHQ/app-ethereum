#ifndef _COMMON_UI_H_
#define _COMMON_UI_H_

#include <stdbool.h>
#include <stdint.h>

void ui_idle(void);
void ui_warning_contract_data(void);
void ui_display_public_eth2(void);
void ui_display_privacy_public_key(void);
void ui_display_privacy_shared_secret(void);
void ui_display_public_key(const uint64_t *chain_id);
void ui_sign_712_v0(void);
void ui_confirm_selector(void);
void ui_confirm_parameter(void);
void app_quit(void);

// EIP-191
void ui_191_start(void);
void ui_191_switch_to_message(void);
void ui_191_switch_to_message_end(void);
void ui_191_switch_to_sign(void);
void ui_191_switch_to_question(void);

// EIP-712
void ui_712_start(void);
void ui_712_switch_to_message(void);
void ui_712_switch_to_sign(void);

#include "ui_callbacks.h"
#include <string.h>

#endif  // _COMMON_UI_H_
