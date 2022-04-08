#include <stdio.h>
#include <stdlib.h>
#include "field_hash.h"
#include "encode_field.h"
#include "path.h"
#include "eip712.h"

const uint8_t *field_hash(const void *const structs_array,
                          const uint8_t *const data,
                          const uint8_t data_length)
{
    const char *type;
    uint8_t typelen;
    const char *key;
    uint8_t keylen;
    const void *field;

    (void)structs_array;
    (void)data;
    (void)data_length;
    // get field by path
    //encode_integer(data, data_length);
    field = path_get_field();
    if (field != NULL)
    {
        printf("==> ");
        type = get_struct_field_typename(field, &typelen);
        fwrite(type, sizeof(char), typelen, stdout);
        printf(" ");
        key = get_struct_field_keyname(field, &keylen);
        fwrite(key, sizeof(char), keylen, stdout);
        printf("\n");
        path_advance();
    }
    return NULL;
}
