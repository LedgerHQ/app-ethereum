#ifndef FILTERING_H_
#define FILTERING_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    FILTERING_CONTRACT_NAME,
    FILTERING_STRUCT_FIELD
}   e_filtering_type;

bool    provide_filtering_info(const uint8_t *const payload,
                               uint8_t length,
                               e_filtering_type type);

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // FILTERING_H_
