#include "fuzz_utils.h"

int fuzzProxyInfo(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    return handle_proxy_info(data[0], 0, size - 1, data + 1);
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();

    // Run the harness
    fuzzProxyInfo(data, size);

    return 0;
}
