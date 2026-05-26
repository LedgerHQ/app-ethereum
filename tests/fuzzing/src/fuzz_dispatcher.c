#include "mocks.h"

#include "fuzz_runtime.h"
#include "fuzz_apdu_adapter.h"
#include "fuzz_command_registry.h"
#include "fuzz_tlv_config.h"
#include "fuzz_harness.h"

size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t max_size, unsigned int seed) {
    return fuzz_ethereum_custom_mutator(data, size, max_size, seed);
}

void fuzz_app_reset(void) {
    fuzz_reset_runtime_state();
}

void fuzz_app_dispatch(void *cmd_v) {
    fuzz_dispatch_command((command_t *) cmd_v);
}

int fuzz_entry(const uint8_t *data, size_t size) {
    return fuzz_harness_entry(data, size);
}
