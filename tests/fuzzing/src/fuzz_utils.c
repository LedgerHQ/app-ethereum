#include "fuzz_utils.h"

#include "caller_api.h"
#include "net_icons.gen.h"

// Global state required by the app features
cx_sha3_t global_sha3 = {0};
cx_sha3_t sha3 = {0};
tmpContent_t tmpContent = {0};
txContext_t txContext = {0};
txContent_t txContent = {0};
dataContext_t dataContext = {0};
tmpCtx_t tmpCtx = {0};
strings_t strings = {0};
caller_app_t *caller_app = NULL;
const chain_config_t *chainConfig = NULL;

const network_icon_t g_network_icons[10] = {0};

uint16_t apdu_response_code = 0;
pluginType_t pluginType = 0;
uint32_t eth2WithdrawalIndex = 0;
uint8_t appState = 0;

// Mock the storage to enable wanted features
const internalStorage_t N_storage_real = {
    .tx_check_enable = true,
    .tx_check_opt_in = true,
    .eip7702_enable = true,
};

chain_config_t config = {
    .coinName = "FUZZ",
    .chainId = 0x42,
};

void reset_app_context(void) {
    gcs_cleanup();
    clear_safe_account();
    ui_all_cleanup();
}

void init_fuzzing_environment(void) {
    // Clear global structures to ensure a clean state for each fuzzing iteration
    explicit_bzero(&global_sha3, sizeof(global_sha3));
    explicit_bzero(&sha3, sizeof(sha3));
    explicit_bzero(&tmpContent, sizeof(tmpContent_t));
    explicit_bzero(&txContext, sizeof(txContext_t));
    explicit_bzero(&txContent, sizeof(txContent_t));
    explicit_bzero(&dataContext, sizeof(dataContext_t));
    explicit_bzero(&tmpCtx, sizeof(tmpCtx_t));
    explicit_bzero(&strings, sizeof(strings_t));

    explicit_bzero(&G_io_apdu_buffer, OS_IO_SEPH_BUFFER_SIZE + 1);

    chainConfig = &config;
    txContext.content = &txContent;
    txContext.sha3 = &sha3;
    pluginType = PLUGIN_TYPE_EXTERNAL;
    eth2WithdrawalIndex = 0;
    appState = APP_STATE_IDLE;
}
