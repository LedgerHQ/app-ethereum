#ifndef EIP712_CTX_H_
#define EIP712_CTX_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include "common_utils.h"

typedef struct {
    uint8_t contract_addr[ADDRESS_LENGTH];
    uint64_t chain_id;
    uint8_t schema_hash[224 / 8];
    bool go_home_on_failure;
} s_eip712_context;

extern s_eip712_context *eip712_context;

bool eip712_context_init(void);
void eip712_context_deinit(void);

typedef enum { NOT_INITIALIZED, INITIALIZED, DEFINED } e_struct_init;
extern e_struct_init struct_state;

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // EIP712_CTX_H_
