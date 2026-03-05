#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"

typedef struct {
    uint8_t weight;
    bool has_start;
    int16_t start;
    bool has_end;
    int16_t end;
} s_array_args;

typedef struct {
    s_array_args *args;
} s_path_array_context;

bool handle_array_struct(const buffer_t *buf, s_path_array_context *context);
