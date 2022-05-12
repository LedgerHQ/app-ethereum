#ifndef EIP712_CTX_H_
#define EIP712_CTX_H_

#include <stdbool.h>


extern uint8_t  *typenames_array;
extern uint8_t  *structs_array;
extern uint8_t  *current_struct_fields_array;

bool    eip712_context_init(void);
void    eip712_context_deinit(void);

extern bool eip712_context_initialized;

#endif // EIP712_CTX_H_
