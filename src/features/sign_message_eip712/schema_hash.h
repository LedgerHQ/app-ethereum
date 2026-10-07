#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lcx_sha256.h"

bool compute_schema_hash(uint8_t hash[CX_SHA224_SIZE]);
