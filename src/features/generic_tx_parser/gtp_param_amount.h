#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    s_value value;
} s_param_amount;

typedef struct {
    s_param_amount *param;
} s_param_amount_context;

bool handle_param_amount_struct(const buffer_t *buf, s_param_amount_context *context);
bool format_param_amount(const s_param_amount *param, const char *name);
