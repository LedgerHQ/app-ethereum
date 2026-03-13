#include <setjmp.h>
#include "fuzz_utils.h"
#include "mocks.h"

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
        if (handle_sign_eip7702_authorization(p1, data + offset, len, &flags) != SWO_SUCCESS)
            return 0;
        offset += len;
    }
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();
    if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) return 0;

    fuzzEIP7702(data, size);

    return 0;
}
