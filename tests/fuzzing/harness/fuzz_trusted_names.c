#include "fuzz_utils.h"

int fuzzTrustedNames(const uint8_t *data, size_t size) {
    size_t offset = 0;
    size_t len = 0;
    uint8_t p1;

    while (size - offset > 3) {
        if (data[offset++] == 0) break;
        p1 = data[offset++];
        len = data[offset++];
        if (size - offset < len) return 0;
        if (handle_trusted_name(p1, data + offset, len) != SWO_SUCCESS) return 0;
        offset += len;
    }
    return 0;
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();

    // Run the harness
    fuzzTrustedNames(data, size);

    return 0;
}
