#ifndef GTP_PARAM_RAW_H_
#define GTP_PARAM_RAW_H_

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    s_value value;
} s_param_raw;

typedef struct {
    s_param_raw *param;
} s_param_raw_context;

bool handle_param_raw_struct(const s_tlv_data *data, s_param_raw_context *context);
bool format_param_raw(const s_param_raw *param, const char *name);

#endif  // !GTP_PARAM_RAW_H_
