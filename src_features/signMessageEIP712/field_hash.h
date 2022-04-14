#ifndef FIELD_HASH_H_
#define FIELD_HASH_H_

#include <stdint.h>
#include <stdbool.h>

#define IS_DYN(type)    (((type) == TYPE_SOL_STRING) || ((type) == TYPE_SOL_BYTES_DYN))

typedef enum
{
    FHS_IDLE,
    FHS_WAITING_FOR_MORE
}   e_field_hashing_state;

typedef struct
{
    uint16_t    remaining_size;
    uint8_t     state; // e_field_hashing_state
}   s_field_hashing;

const uint8_t *field_hash(const uint8_t *data,
                          uint8_t data_length,
                          bool partial);
#endif // FIELD_HASH_H_
