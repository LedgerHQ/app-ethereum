#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    uint8_t id;
    s_value value;
} s_param_enum;

typedef struct {
    s_param_enum *param;
} s_param_enum_context;

bool handle_param_enum_struct(const s_tlv_data *data, s_param_enum_context *context);
bool format_param_enum(const s_param_enum *param, const char *name);
