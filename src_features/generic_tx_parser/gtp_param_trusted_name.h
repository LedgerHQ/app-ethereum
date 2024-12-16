#ifndef GTP_PARAM_TRUSTED_NAME_H_
#define GTP_PARAM_TRUSTED_NAME_H_

#ifdef HAVE_TRUSTED_NAME

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_value.h"
#include "trusted_name.h"

typedef struct {
    uint8_t version;
    s_value value;
    uint8_t type_count;
    e_name_type types[TN_TYPE_COUNT];
    uint8_t source_count;
    e_name_source sources[TN_SOURCE_COUNT];
} s_param_trusted_name;

typedef struct {
    s_param_trusted_name *param;
} s_param_trusted_name_context;

bool handle_param_trusted_name_struct(const s_tlv_data *data,
                                      s_param_trusted_name_context *context);
bool format_param_trusted_name(const s_param_trusted_name *param, const char *name);

#endif

#endif  // !GTP_PARAM_TRUSTED_NAME_H_
