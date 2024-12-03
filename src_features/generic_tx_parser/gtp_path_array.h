#ifndef GTP_PATH_ARRAY_H_
#define GTP_PATH_ARRAY_H_

#include "tlv.h"

typedef struct {
    uint8_t weight;
    bool subset;
    int16_t start;
    uint16_t length;
} s_array_args;

typedef struct {
    s_array_args *args;
} s_path_array_context;

bool handle_array_struct(const s_tlv_data *data, s_path_array_context *context);

#endif // GTP_PATH_ARRAY_H_
