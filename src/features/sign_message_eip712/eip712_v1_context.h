#pragma once

#include <stdbool.h>
#include "common_utils.h"
#include "lists.h"
#include "lcx_sha256.h"

typedef struct {
    uint8_t schema_hash[CX_SHA224_SIZE];
    bool sign_claimed;
} s_eip712_v1_context;

extern s_eip712_v1_context *eip712_v1_context;

bool eip712_v1_context_init(void);
void eip712_v1_context_deinit(void);
bool eip712_v1_sign_claim(void);
