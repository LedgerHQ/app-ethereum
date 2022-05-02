#include <stdlib.h>
#include "field_hash.h"
#include "encode_field.h"

const uint8_t *field_hash(const void *const structs_array,
                          const uint8_t *const data,
                          const uint8_t data_length)
{
    (void)structs_array;
    (void)data;
    (void)data_length;
    // get field by path
    encode_integer(data, data_length);
    // path += 1
    return NULL;
}
