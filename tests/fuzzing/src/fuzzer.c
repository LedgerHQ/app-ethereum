#include <stdint.h>
#include <stdlib.h>

#include "cmd_trusted_name.h"

#include "cmd_network_info.h"

#include "cmd_get_tx_simulation.h"

#include "cmd_proxy_info.h"

#include "cmd_field.h"
#include "cmd_tx_info.h"
#include "cmd_enum_value.h"

#include "gtp_field.h"
#include "gtp_tx_info.h"
#include "enum_value.h"

#include "auth_7702.h"
#include "commands_7702.h"

#include "shared_context.h"
#include "tlv.h"
#include "apdu_constants.h"

// Fuzzing harness interface
typedef int (*harness)(const uint8_t *data, size_t size);

// Global state required by the app features
cx_sha3_t global_sha3;
cx_sha3_t sha3;
unsigned char G_io_apdu_buffer[260];
tmpContent_t tmpContent;
txContext_t txContext;
txContent_t txContent;
chain_config_t config = {
    .coinName = "FUZZ",
    .chainId = 0x42,
};
const chain_config_t *chainConfig = &config;
uint8_t appState;
tmpCtx_t tmpCtx;
strings_t strings;
// Mock the storage to enable wanted features
const internalStorage_t N_storage_real = {
    .tx_check_enable = true,
    .tx_check_opt_in = true,
    .eip7702_enable = true,
};

int fuzzGenericParserFieldCmd(const uint8_t *data, size_t size) {
    s_field field = {0};
    s_field_ctx ctx = {0};
    ctx.field = &field;

    if (!tlv_parse(data, size, (f_tlv_data_handler) &handle_field_struct, &ctx)) return 1;

    if (!verify_field_struct(&ctx)) return 1;

    return format_field(&field);
}

int fuzzGenericParserTxInfoCmd(const uint8_t *data, size_t size) {
    s_tx_info tx_info = {0};
    s_tx_info_ctx ctx = {0};
    ctx.tx_info = &tx_info;

    if (!tlv_parse(data, size, (f_tlv_data_handler) &handle_tx_info_struct, &ctx)) return 1;

    return verify_tx_info_struct(&ctx);
}

int fuzzGenericParserEnumCmd(const uint8_t *data, size_t size) {
    s_enum_value_ctx ctx = {0};

    if (!tlv_parse(data, size, (f_tlv_data_handler) &handle_enum_value_struct, &ctx)) return 1;

    return verify_enum_value_struct(&ctx);
}

int fuzzDynamicNetworks(const uint8_t *data, size_t size) {
    size_t offset = 0;
    size_t len = 0;
    uint8_t p1;
    uint8_t p2;
    unsigned int tx;

    while (size - offset > 4) {
        if (data[offset++] == 0) break;
        p1 = data[offset++];
        p2 = data[offset++];
        len = data[offset++];
        if (size - offset < len) return 0;
        if (handle_network_info(p1, p2, data + offset, len, &tx) != APDU_RESPONSE_OK) return 1;
        offset += len;
    }
    return 0;
}

int fuzzTrustedNames(const uint8_t *data, size_t size) {
    size_t offset = 0;
    size_t len = 0;
    uint8_t p1;

    while (size - offset > 3) {
        if (data[offset++] == 0) break;
        p1 = data[offset++];
        len = data[offset++];
        if (size - offset < len) return 0;
        if (handle_trusted_name(p1, data + offset, len) != APDU_RESPONSE_OK) return 1;
        offset += len;
    }
    return 0;
}

int fuzzNFTInfo(const uint8_t *data, size_t size) {
    unsigned int tx;
    return handleProvideNFTInformation(data, size, &tx) != APDU_RESPONSE_OK;
}

int fuzzProxyInfo(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    return handle_proxy_info(data[0], 0, size - 1, data + 1);
}

int fuzzTxSimulation(const uint8_t *data, size_t size) {
    unsigned int flags;
    if (size < 2) return 0;

    if (handle_tx_simulation(data[0], data[1], data + 2, size - 2, &flags) != APDU_RESPONSE_OK)
        return 1;

    get_tx_simulation_risk_str();
    get_tx_simulation_category_str();
    return 0;
}

int fuzzCalldata(const uint8_t *data, size_t size) {
    while (size > 0) {
        switch (data[0]) {
            case 'I':
                data++;
                size--;
                calldata_init(500);
                break;
            case 'W':
                size--;
                data++;
                if (size < 1 || size < data[0] + 1) return 0;
                calldata_append(data + 1, data[0]);
                size -= (1 + data[0]);
                data += 1 + data[0];
                break;
            case 'R':
                size--;
                data++;
                if (size < 1) return 0;
                calldata_get_chunk(data[0]);
                size--;
                data++;
                break;
            default:
                return 0;
        }
    }
    return 0;
}

int fuzzEIP7702(const uint8_t *data, size_t size) {
    size_t offset = 0;
    size_t len = 0;
    uint8_t p1;
    unsigned int flags;

    while (size - offset > 3) {
        if (data[offset++] == 0) break;
        p1 = data[offset++];
        len = data[offset++];
        if (size - offset < len) return 0;
        if (handleSignEIP7702Authorization(p1, data + offset, len, &flags) != APDU_RESPONSE_OK)
            return 1;
        offset += len;
    }
    return 0;
}

// Array of fuzzing harness functions
harness harnesses[] = {
    fuzzGenericParserFieldCmd,
    fuzzGenericParserTxInfoCmd,
    fuzzGenericParserEnumCmd,
    fuzzDynamicNetworks,
    fuzzTrustedNames,
    fuzzNFTInfo,
    fuzzProxyInfo,
    fuzzTxSimulation,
    fuzzCalldata,
    fuzzEIP7702,
};

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Clear global structures to ensure a clean state for each fuzzing iteration
    explicit_bzero(&tmpContent, sizeof(tmpContent_t));
    explicit_bzero(&txContext, sizeof(txContext_t));
    explicit_bzero(&txContent, sizeof(txContent_t));
    explicit_bzero(&tmpCtx, sizeof(tmpCtx_t));
    explicit_bzero(&strings, sizeof(strings_t));
    explicit_bzero(&G_io_apdu_buffer, 260);
    explicit_bzero(&sha3, sizeof(sha3));
    explicit_bzero(&global_sha3, sizeof(global_sha3));

    calldata_cleanup();

    uint8_t target;

    txContext.content = &txContent;
    txContext.sha3 = &sha3;

    // Determine which harness function to call based on the first byte of data
    if (size < 1) return 0;
    target = data[0];
    if (target >= sizeof(harnesses) / sizeof(harnesses[0])) return 0;

    // Call the selected harness function with the remaining data (which can be of size 0)
    return harnesses[target](++data, --size);
}
