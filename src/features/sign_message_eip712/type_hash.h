#pragma once

#include <stdint.h>
#include <stdbool.h>

bool type_hash(const char *struct_name, const uint8_t struct_name_length, uint8_t *hash_buf);
