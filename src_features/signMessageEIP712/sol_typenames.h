#pragma once

#include <stdbool.h>
#include <stdint.h>

bool sol_typenames_init(void);

const char *get_struct_field_sol_typename(const uint8_t *ptr, uint8_t *const length);
