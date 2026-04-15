#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"
#include "gtp_value.h"

typedef enum {
    DT_UNIX = 0,
    DT_BLOCKHEIGHT = 1,
} e_datetime_type;

typedef struct {
    uint8_t version;
    s_value value;
    e_datetime_type type;
} s_param_datetime;

typedef struct {
    s_param_datetime *param;
} s_param_datetime_context;

bool handle_param_datetime_struct(const buffer_t *buf, s_param_datetime_context *context);
bool format_param_datetime(const s_param_datetime *param, const char *name);
