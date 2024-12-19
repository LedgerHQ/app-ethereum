#ifndef GTP_PARAM_UNIT_H_
#define GTP_PARAM_UNIT_H_

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    s_value value;
    char base[11];
    uint8_t decimals;
} s_param_unit;

typedef struct {
    s_param_unit *param;
} s_param_unit_context;

bool handle_param_unit_struct(const s_tlv_data *data, s_param_unit_context *context);
bool format_param_unit(const s_param_unit *param, const char *name);

#endif  // !GTP_PARAM_UNIT_H_
