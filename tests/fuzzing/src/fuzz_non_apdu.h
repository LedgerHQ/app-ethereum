#pragma once

#include <stdbool.h>

#include "parser.h"

void fuzz_non_apdu_reset(void);
bool fuzz_dispatch_extension(const command_t *cmd);
