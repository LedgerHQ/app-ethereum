#pragma once

#include <stdbool.h>
#include "buffer.h"
#include "typed_data.h"

bool handle_eip712_v2_schema_field_struct(const buffer_t *buf, s_struct_712_field **out);
