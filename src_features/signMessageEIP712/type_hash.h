#ifndef TYPE_HASH_H_
#define TYPE_HASH_H_

#include <stdint.h>
#include <stdbool.h>

bool type_hash(const char *const struct_name, const uint8_t struct_name_length, uint8_t *hash_buf);

#endif  // TYPE_HASH_H_
