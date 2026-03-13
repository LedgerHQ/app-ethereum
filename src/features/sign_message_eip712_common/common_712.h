#pragma once

#include <stdint.h>
#include "ui_logic.h"

uint16_t ui_712_start(e_eip712_filtering_mode filtering);

void eip712_format_hash(uint8_t index);

void ui_712_approve_cb(void);
void ui_712_reject_cb(void);
