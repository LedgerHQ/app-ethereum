#pragma once

#include <stdbool.h>
#include <stdint.h>

bool ui_712_prepare_review(void);
bool ui_712_prepare_blind_review(void);

bool eip712_format_hash(uint8_t index);
void eip712_hash_strs_cleanup(void);

void ui_712_approve_cb(void);
void ui_712_reject_cb(void);
