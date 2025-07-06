#pragma once

#include <stdint.h>

#define MAX_EXPLORER_LEN 64

typedef struct {
    uint64_t chain_id;
    char name[MAX_EXPLORER_LEN];
} explorer_info_t;