#pragma once

#include "cx_errors.h"
#include "ox_ec.h"
#include "os_task.h"
#include <string.h>
#include <setjmp.h>
#include "exceptions.h"
#include <stdio.h>
#include <stdint.h>

#include "fuzz_defs.h"

extern try_context_t fuzz_exit_jump_ctx;

#define FUZZ_CTRL_SIZE 16
extern uint8_t fuzz_ctrl[FUZZ_CTRL_SIZE];

extern const uint8_t *fuzz_tail_ptr;
extern size_t fuzz_tail_len;
