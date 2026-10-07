#pragma once

#include "cx_errors.h"
#include "lcx_sha3.h"  // cx_sha3_t
#include "ox_ec.h"
#include "os_task.h"
#include <string.h>
#include <setjmp.h>
#include "exceptions.h"
#include <stdio.h>
#include <stdint.h>

#include "fuzz_defs.h"

/**
 * Keccak context the harnesses point txContext.sha3 at. The app allocates its
 * own on the heap (g_tx_hash_ctx), which a restored prefix cannot own, so the
 * fuzz build supplies this one. Defined in mock/app_globals.c.
 */
extern cx_sha3_t global_sha3;
