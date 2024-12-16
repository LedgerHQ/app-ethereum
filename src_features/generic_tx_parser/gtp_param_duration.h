#ifndef GTP_PARAM_DURATION_H_
#define GTP_PARAM_DURATION_H_

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    s_value value;
} s_param_duration;

typedef struct {
    s_param_duration *param;
} s_param_duration_context;

bool handle_param_duration_struct(const s_tlv_data *data, s_param_duration_context *context);
bool format_param_duration(const s_param_duration *param, const char *name);

#endif  // !GTP_PARAM_DURATION_H_
