#ifndef GTP_PATH_SLICE_H_
#define GTP_PATH_SLICE_H_

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"

typedef struct {
    bool has_start;
    int16_t start;
    bool has_end;
    int16_t end;
} s_slice_args;

typedef struct {
    s_slice_args *args;
} s_path_slice_context;

bool handle_slice_struct(const s_tlv_data *data, s_path_slice_context *context);

#endif  // !GTP_PATH_SLICE_H_
