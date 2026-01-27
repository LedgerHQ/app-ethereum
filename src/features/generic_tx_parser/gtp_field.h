#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "tlv.h"
#include "gtp_param_raw.h"
#include "gtp_param_amount.h"
#include "gtp_param_token_amount.h"
#include "gtp_param_nft.h"
#include "gtp_param_datetime.h"
#include "gtp_param_duration.h"
#include "gtp_param_unit.h"
#include "gtp_param_enum.h"
#include "gtp_param_trusted_name.h"
#include "gtp_param_calldata.h"
#include "gtp_param_token.h"
#include "gtp_param_network.h"
#include "calldata.h"
#include "list.h"

typedef enum {
    PARAM_TYPE_RAW = 0,
    PARAM_TYPE_AMOUNT,
    PARAM_TYPE_TOKEN_AMOUNT,
    PARAM_TYPE_NFT,
    PARAM_TYPE_DATETIME,
    PARAM_TYPE_DURATION,
    PARAM_TYPE_UNIT,
    PARAM_TYPE_ENUM,
    PARAM_TYPE_TRUSTED_NAME,
    PARAM_TYPE_CALLDATA,
    PARAM_TYPE_TOKEN,
    PARAM_TYPE_NETWORK,
    PARAM_TYPE_INTENT,
} e_param_type;

typedef enum {
    PARAM_VISIBILITY_ALWAYS = 0,
    PARAM_VISIBILITY_MUST_BE,
    PARAM_VISIBILITY_IF_NOT_IN,
    PARAM_VISIBILITY_MAX,
} e_param_visibility;

typedef struct s_field_constraint {
    s_flist_node node;
    uint8_t size;
    uint8_t *value;
} s_field_constraint;

typedef struct s_field {
    uint8_t version;
    char name[21];
    e_param_type param_type;
    union {
        s_param_raw param_raw;
        s_param_amount param_amount;
        s_param_token_amount param_token_amount;
        s_param_nft param_nft;
        s_param_datetime param_datetime;
        s_param_duration param_duration;
        s_param_unit param_unit;
        s_param_enum param_enum;
        s_param_trusted_name param_trusted_name;
        s_param_calldata param_calldata;
        s_param_token param_token;
        s_param_network param_network;
    };
    e_param_visibility visibility;
    s_field_constraint *constraints;
} s_field;

typedef struct {
    s_field *field;
    uint8_t set_flags;
} s_field_ctx;

bool handle_field_struct(const s_tlv_data *data, s_field_ctx *context);
bool verify_field_struct(const s_field_ctx *context);
bool format_field(s_field *field);
void cleanup_field_constraints(s_field *field);
