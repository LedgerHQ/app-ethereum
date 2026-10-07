/* Shared app globals that the fuzz build owns instead of `src/main.c`.
 *
 * Every target links this single translation unit so a global like `tmpCtx` or
 * `txContext` has exactly one definition. Per-iteration setup (keccak context,
 * chain config, ...) is done by each harness's reset: src/fuzz_dispatcher.c for
 * fuzz_app, src/fuzz_utils.c for the per-feature targets. */

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

/* The device defaults, so the fuzzer sees the settings a user runs with.
 * contractDetails matters most: with it set, custom_processor() takes the
 * raw-calldata display branch and never calls eth_plugin_perform_init(), which
 * puts the whole plugin tree out of reach from INS_SIGN. */
const internalStorage_t N_storage_real = {
    .dataAllowed = true,
    .contractDetails = false,
    .tx_check_enable = true,
    .tx_check_opt_in = true,
    .eip7702_enable = true,
};

const caller_app_t *g_caller_app = NULL;
const chain_config_t *g_chain_config = NULL;

// Stable chain config every harness points g_chain_config at. Production sets it
// at init, so the fuzzer must not leave it NULL (a random/zero pointer here
// crashes readers like app_compatible_with_chain_id).
const chain_config_t g_fuzz_chain_config = {
    .ticker = "FUZZ",
    .chain_id = 1,
    .coin_type = 60,
};
