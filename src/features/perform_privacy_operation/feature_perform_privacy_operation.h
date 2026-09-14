#pragma once

#include "shared_context.h"

uint32_t set_result_perform_privacy_operation(void);

// Wipe the staged secret material and release the privacy app state. Safe to
// call after a status has been sent.
void privacy_operation_cleanup(void);
