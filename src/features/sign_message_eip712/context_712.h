#pragma once

#include <stdbool.h>
#include "common_utils.h"
#include "lists.h"
#include "lcx_sha256.h"

typedef struct {
    uint8_t contract_addr[ADDRESS_LENGTH];
    uint64_t chain_id;
    uint8_t schema_hash[CX_SHA224_SIZE];
    bool go_home_on_failure;
} s_eip712_context;

extern s_eip712_context *eip712_context;

bool eip712_context_init(void);
void eip712_context_deinit(void);

typedef enum { NOT_INITIALIZED, INITIALIZED, DEFINED } e_struct_init;
extern e_struct_init struct_state;
