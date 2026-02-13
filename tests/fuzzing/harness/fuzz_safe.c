#include "fuzz_utils.h"

#include "safe_descriptor.h"

int fuzzSafeCmd(const uint8_t *data, size_t size) {
    buffer_t buf = {.ptr = (uint8_t *) data, .size = size, .offset = 0};
    handle_safe_tlv_payload(&buf);
    return 0;
}

int fuzzSignerCmd(const uint8_t *data, size_t size) {
    if (size < 3) return 0;
    safe_descriptor_t desc = {
        .address = "AAAAAAAAAAAAAAAAAAAA",
        .threshold = data[0] % 100,
        .signers_count = data[1] % 100,
        .role = data[2] % 2,
    };
    SAFE_DESC = &desc;
    buffer_t buf = {.ptr = (uint8_t *) data + 3, .size = size - 3, .offset = 0};
    handle_signer_tlv_payload(&buf);
    SAFE_DESC = NULL;
    return 0;
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t target;
    init_fuzzing_environment();
    SAFE_DESC = NULL;

    // Determine which harness function to call based on the first byte of data
    if (size < 1) return 0;
    target = data[0];
    data++;
    size--;
    if (target & 0x01) {
        fuzzSafeCmd(data, size);
    } else {
        fuzzSignerCmd(data, size);
    }

    return 0;
}
