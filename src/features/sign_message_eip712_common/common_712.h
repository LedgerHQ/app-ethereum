#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "eip712_v1_ui_logic.h"

bool ui_712_start(e_eip712_filtering_mode filtering);

void eip712_format_hash(uint8_t index);
void eip712_hash_strs_cleanup(void);

void ui_712_approve_cb(void);
void ui_712_reject_cb(void);
