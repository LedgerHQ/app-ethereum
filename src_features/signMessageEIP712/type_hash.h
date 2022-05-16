#ifndef TYPE_HASH_H_
#define TYPE_HASH_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdint.h>

const uint8_t *type_hash(const void *const structs_array,
                         const char *const struct_name,
                         const uint8_t struct_name_length);

#endif // HAVE_EIP712_FULL_SUPPORT

#endif // TYPE_HASH_H_
