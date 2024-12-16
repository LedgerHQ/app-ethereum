#ifndef GTP_FIELD_H_
#define GTP_FIELD_H_

#include <stdbool.h>
#include <stdint.h>
#include "tlv.h"
#include "gtp_param_raw.h"
#include "gtp_param_amount.h"

typedef enum {
    PARAM_TYPE_RAW = 0,
    PARAM_TYPE_AMOUNT,
} e_param_type;

typedef struct {
    uint8_t version;
    char name[21];
    e_param_type param_type;
    union {
        s_param_raw param_raw;
        s_param_amount param_amount;
    };
} s_field;

typedef struct {
    s_field *field;
    uint8_t set_flags;
} s_field_ctx;

bool handle_field_struct(const s_tlv_data *data, s_field_ctx *context);
bool verify_field_struct(const s_field_ctx *context);
bool format_field(const s_field *field);

#endif  // !GTP_FIELD_H_
