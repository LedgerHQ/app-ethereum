#include "fuzz_utils.h"

#include "gtp_field.h"
#include "gtp_tx_info.h"
#include "enum_value.h"
#include "buffer.h"

// Fuzzing harness interface
typedef int (*harness)(const uint8_t *data, size_t size);

int fuzzGenericParserFieldCmd(const uint8_t *data, size_t size) {
    s_field field = {0};
    s_field_ctx ctx = {0};
    ctx.field = &field;

    // Use buffer_t with lib_tlv API
    buffer_t buf = {.ptr = (uint8_t *) data, .size = size, .offset = 0};
    if (!handle_field_struct(&buf, &ctx)) {
        cleanup_field_constraints(&field);
        return 0;
    }

    if (!verify_field_struct(&ctx)) {
        cleanup_field_constraints(&field);
        return 0;
    }

    // format_field() always cleans up constraints internally (success or failure)
    return format_field(&field);
}

int fuzzGenericParserTxInfoCmd(const uint8_t *data, size_t size) {
    s_tx_info tx_info = {0};
    s_tx_info_ctx ctx = {0};
    ctx.tx_info = &tx_info;

    cx_sha256_init(&ctx.struct_hash);

    // Use buffer_t with lib_tlv API
    buffer_t buf = {.ptr = (uint8_t *) data, .size = size, .offset = 0};
    if (!handle_tx_info_struct(&buf, &ctx)) return 0;

    return verify_tx_info_struct(&ctx);
}

int fuzzGenericParserEnumCmd(const uint8_t *data, size_t size) {
    s_enum_value_ctx ctx = {0};

    cx_sha256_init(&ctx.hash_ctx);

    // Use buffer_t with lib_tlv API
    buffer_t buf = {.ptr = (uint8_t *) data, .size = size, .offset = 0};
    if (!handle_enum_value_tlv_payload(&buf, &ctx)) return 0;

    return verify_enum_value_struct(&ctx);
}

// Array of fuzzing harness functions
harness harnesses[] = {
    fuzzGenericParserFieldCmd,
    fuzzGenericParserTxInfoCmd,
    fuzzGenericParserEnumCmd,
};

/* Main fuzzing handler called by libfuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t target;
    init_fuzzing_environment();

    // Determine which harness function to call based on the first byte of data
    if (size < 1) return 0;
    target = data[0];
    if (target >= sizeof(harnesses) / sizeof(harnesses[0])) return 0;

    // Call the selected harness function with the remaining data (which can be of size 0)
    harnesses[target](++data, --size);

    return 0;
}
