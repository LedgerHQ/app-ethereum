#ifndef FIELD_HASH_H_
#define FIELD_HASH_H_

#include <stdint.h>

const uint8_t *field_hash(const void *const structs_array,
                          const uint8_t *const data,
                          const uint8_t data_length);
#endif // FIELD_HASH_H_
