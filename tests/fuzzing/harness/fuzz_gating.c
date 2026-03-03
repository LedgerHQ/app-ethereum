#include "fuzz_utils.h"
#include "cmd_get_gating.h"

int fuzzGating(const uint8_t *data, size_t size) {
    size_t offset = 0;
    size_t len = 0;
    uint8_t p1;
    uint8_t p2;

    while (size - offset > 4) {
        if (data[offset++] == 0) break;
        p1 = data[offset++];
        p2 = data[offset++];
        len = data[offset++];
        if (size - offset < len) return 0;
        if (handle_gating(p1, p2, data + offset, len) != SWO_SUCCESS) return 0;
        offset += len;
    }
    set_gating_warning();
    return 0;
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();

    fuzzGating(data, size);

    return 0;
}
