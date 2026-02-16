#include "fuzz_utils.h"

int fuzzNFTInfo(const uint8_t *data, size_t size) {
    unsigned int tx;
    return handleProvideNFTInformation(data, size, &tx) != SWO_SUCCESS;
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();

    // Run the harness
    fuzzNFTInfo(data, size);

    return 0;
}
