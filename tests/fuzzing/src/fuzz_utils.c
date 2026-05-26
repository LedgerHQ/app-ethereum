#include "fuzz_utils.h"

#include "caller_app.h"
#include "net_icons.gen.h"
#include "app_mem_utils.h"

/* Per-feature harness scratch state. Shared app globals (tmpCtx, txContext,
 * tmpContent, dataContext, strings, global_sha3, appState, pluginType,
 * eth2WithdrawalIndex, N_storage_real, g_caller_app, g_chain_config) are
 * owned by mock/app_globals.c so both fuzz entry points (per-feature
 * harnesses and the global-coverage dispatcher) share one definition.
 *
 * The two structs below are private to the classic per-feature harnesses
 * and are only referenced from init_fuzzing_environment() to back
 * `txContext.content` and `txContext.sha3`. They are `static` and prefixed
 * `g_fuzz_local_*` to avoid any visual collision with the shared globals
 * `global_sha3` and `tmpContent.txContent` defined in mock/app_globals.c. */
static cx_sha3_t g_fuzz_local_sha3 = {0};
static txContent_t g_fuzz_local_tx_content = {0};

const network_icon_t g_network_icons[10] = {0};

static const chain_config_t fuzz_chain_config = {
    .ticker = "FUZZ",
    .chain_id = 0x42,
    .coin_type = 60,
};

void init_fuzzing_environment(void) {
    static bool mem_initialized = false;
    if (!mem_initialized) {
        static uint8_t heap_buffer[16 * 1024];
        mem_utils_init(heap_buffer, sizeof(heap_buffer));
        mem_initialized = true;
    }

    explicit_bzero(&global_sha3, sizeof(global_sha3));
    explicit_bzero(&g_fuzz_local_sha3, sizeof(g_fuzz_local_sha3));
    explicit_bzero(&tmpContent, sizeof(tmpContent_t));
    explicit_bzero(&txContext, sizeof(txContext_t));
    explicit_bzero(&g_fuzz_local_tx_content, sizeof(g_fuzz_local_tx_content));
    explicit_bzero(&dataContext, sizeof(dataContext_t));
    explicit_bzero(&tmpCtx, sizeof(tmpCtx_t));
    explicit_bzero(&strings, sizeof(strings_t));

    explicit_bzero(&G_io_tx_buffer, OS_IO_SEPH_BUFFER_SIZE + 1);

    g_chain_config = &fuzz_chain_config;
    txContext.content = &g_fuzz_local_tx_content;
    txContext.sha3 = &g_fuzz_local_sha3;
    pluginType = PLUGIN_TYPE_EXTERNAL;
    eth2WithdrawalIndex = 0;
    appState = APP_STATE_IDLE;
}
