#ifndef EIP712_CTX_H_
#define EIP712_CTX_H_

#include <stdbool.h>


extern uint8_t  *typenames_array;
extern uint8_t  *structs_array;
extern uint8_t  *current_struct_fields_array;

bool    init_eip712_context(void);

#endif // EIP712_CTX_H_
