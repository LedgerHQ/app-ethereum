#ifndef FILTERING_H_
#define FILTERING_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

#define FILTERING_MAGIC_CONTRACT_NAME 0b10110111  //  183
#define FILTERING_MAGIC_STRUCT_FIELD  0b01001000  // ~183 = 72

typedef enum { FILTERING_PROVIDE_MESSAGE_INFO, FILTERING_SHOW_FIELD } e_filtering_type;

bool provide_filtering_info(const uint8_t *const payload, uint8_t length, e_filtering_type type);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // FILTERING_H_
