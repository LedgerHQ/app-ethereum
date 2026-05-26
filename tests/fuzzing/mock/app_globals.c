/* Shared app globals that the fuzz build owns instead of `src/main.c`.
 *
 * Both the per-feature fuzz harnesses and the SDK dispatcher harness link
 * this single translation unit so a global like `tmpCtx` or `txContext` has
 * exactly one definition. Per-flow scratch (e.g. the classic harnesses'
 * private `sha3` / `txContent`) lives in `src/fuzz_utils.c` and is not
 * defined here. */

#include <stdbool.h>
#include <stdint.h>

#include "shared_context.h"

tmpCtx_t tmpCtx;
txContext_t txContext;
tmpContent_t tmpContent;
dataContext_t dataContext;
strings_t strings;
cx_sha3_t global_sha3;

uint8_t appState;
uint16_t apdu_response_code;
pluginType_t pluginType;

#ifdef HAVE_ETH2
uint32_t eth2WithdrawalIndex;
#endif

const internalStorage_t N_storage_real = {
    .dataAllowed = true,
    .contractDetails = true,
    .tx_check_enable = true,
    .tx_check_opt_in = true,
    .eip7702_enable = true,
};

const caller_app_t *g_caller_app = NULL;
const chain_config_t *g_chain_config = NULL;

uint32_t app_stack_canary;
