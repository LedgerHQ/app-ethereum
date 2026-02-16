#include "fuzz_utils.h"

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
        if (handle_network_info(p1, p2, data + offset, len, &tx) != SWO_SUCCESS) return 0;
        offset += len;
    }
    return 0;
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();

    // Run the harness
    fuzzDynamicNetworks(data, size);

    return 0;
}
