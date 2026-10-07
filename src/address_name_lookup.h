#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "trusted_name.h"

// clang-format off
typedef enum {
    ADDR_NAME_FROM_ADDRESS_BOOK,
    ADDR_NAME_FROM_TRUSTED_NAME,
    ADDR_NAME_FROM_RAW,
} e_addr_name_source;
// clang-format on

bool get_address_display_name(const uint8_t *addr,
                              uint64_t chain_id,
                              uint8_t type_count,
                              const e_name_type *types,
                              uint8_t source_count,
                              const e_name_source *sources,
                              char *buf,
                              size_t buf_size,
                              e_addr_name_source *name_source_out,
                              const void **extra_data_out);
