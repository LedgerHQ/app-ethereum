#include "fuzz_utils.h"

#include "calldata.h"

static s_calldata *g_calldata = NULL;

int fuzzCalldata(const uint8_t *data, size_t size) {
    while (size > 0) {
        switch (data[0]) {
            case 'I':
                data++;
                size--;
                if (g_calldata != NULL) {
                    calldata_delete(g_calldata);
                }
                g_calldata = calldata_init(500, NULL);
                break;
            case 'W':
                size--;
                data++;
                if (size < 1 || size < data[0] + 1) return 0;
                calldata_append(g_calldata, data + 1, data[0]);
                size -= (1 + data[0]);
                data += 1 + data[0];
                break;
            case 'R':
                size--;
                data++;
                if (size < 1) return 0;
                calldata_get_chunk(g_calldata, data[0]);
                size--;
                data++;
                break;
            default:
                return 0;
        }
    }
    return 0;
}

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    init_fuzzing_environment();

    // Run the harness
    fuzzCalldata(data, size);

    return 0;
}
