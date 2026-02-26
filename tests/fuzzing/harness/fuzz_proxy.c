#include <setjmp.h>
#include "fuzz_utils.h"
#include "mocks.h"

int fuzzProxyInfo(const uint8_t *data, size_t size) {
    if (size < 1) return 0;
    return handle_proxy_info(data[0], 0, size - 1, data + 1);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();
    if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) return 0;

    fuzzProxyInfo(data, size);

    return 0;
}
