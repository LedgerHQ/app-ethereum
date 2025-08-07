#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "typed_data.h"

bool sol_typenames_init(void);
void sol_typenames_deinit(void);
const char *get_struct_field_sol_typename(const s_struct_712_field *field_ptr);
