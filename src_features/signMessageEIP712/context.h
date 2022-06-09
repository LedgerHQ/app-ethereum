#ifndef EIP712_CTX_H_
#define EIP712_CTX_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include "ethUstream.h" // ADDRESS_LENGTH

typedef struct
{
    uint8_t *typenames_array;
    uint8_t *structs_array;
    uint8_t *current_struct_fields_array;
    uint8_t contract_addr[ADDRESS_LENGTH];
}   s_eip712_context;

extern s_eip712_context *eip712_context;

bool    eip712_context_init(void);
void    eip712_context_deinit(void);

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // EIP712_CTX_H_
