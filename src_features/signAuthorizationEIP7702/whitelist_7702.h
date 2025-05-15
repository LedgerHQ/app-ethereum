#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "common_utils.h"

#define MAX_NAME_LEN 31  // 30 characters + '\0'

typedef struct eip7702_whitelist_s {
    const uint8_t address[ADDRESS_LENGTH];
    const char name[MAX_NAME_LEN];
    uint64_t chain_id;
} eip7702_whitelist_t;

const char *get_delegate_name(const uint64_t *chain_id, const uint8_t *address);
