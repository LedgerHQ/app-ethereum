#ifndef ENCODE_TYPE_H_
#define ENCODE_TYPE_H_

#include <stdint.h>
#include <stdbool.h>

const char  *encode_type(const void *const structs_array,
                        const char *const struct_name,
                        const uint8_t struct_name_length,
                        uint16_t *const encoded_length);

#endif // ENCODE_TYPE_H_
