#include "fuzz_utils.h"

int fuzzTxCheck(const uint8_t *data, size_t size) {
    unsigned int flags;
    if (size < 2) return 0;

    if (handle_tx_simulation(data[0], data[1], data + 2, size - 2, &flags) != SWO_SUCCESS) return 0;

    get_tx_simulation_risk_str();
    get_tx_simulation_category_str();
    return 0;
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();

    // Run the harness
    fuzzTxCheck(data, size);

    return 0;
}
