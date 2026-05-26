#pragma once

#include <stdint.h>

#include "parser.h"

uint16_t handleApdu(command_t *cmd, uint32_t *tx);
