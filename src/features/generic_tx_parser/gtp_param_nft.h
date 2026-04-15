#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"
#include "gtp_value.h"

typedef struct {
    uint8_t version;
    s_value id;
    s_value collection;
} s_param_nft;

typedef struct {
    s_param_nft *param;
} s_param_nft_context;

bool handle_param_nft_struct(const buffer_t *buf, s_param_nft_context *context);
bool format_param_nft(const s_param_nft *param, const char *name);
