#include <stdint.h>
#include <stdlib.h>

#include "cmd_field.h"
#include "cmd_tx_info.h"
#include "cmd_enum_value.h"

#include "gtp_field.h"
#include "gtp_tx_info.h"
#include "enum_value.h"

#include "shared_context.h"
#include "tlv.h"

cx_sha3_t sha3 = {0};
unsigned char G_io_apdu_buffer[260];
tmpContent_t tmpContent;
txContext_t txContext;
txContent_t txContent;
chain_config_t config = {
    .coinName = "FUZZ",
    .chainId = 0x42,
};
const chain_config_t *chainConfig = &config;
uint8_t appState;
tmpCtx_t tmpCtx;
strings_t strings;


int fuzzGenericParserFieldCmd(const uint8_t *data, size_t size) {
    s_field field = {0};
    s_field_ctx ctx = {0};
    ctx.field = &field;

    if (!tlv_parse(data, size, (f_tlv_data_handler)&handle_field_struct, &ctx))
        return 1;

    if (!verify_field_struct(&ctx))
        return 1;
    
    return format_field(&field);
}

int fuzzGenericParserTxInfoCmd(const uint8_t *data, size_t size) {
    s_tx_info tx_info = {0};
    s_tx_info_ctx ctx = {0};
    ctx.tx_info = &tx_info;

    if (!tlv_parse(data, size, (f_tlv_data_handler)&handle_tx_info_struct, &ctx))
        return 1;
    
    return verify_tx_info_struct(&ctx);
}

int fuzzGenericParserEnumCmd(const uint8_t *data, size_t size) {
    s_enum_value_ctx ctx = {0};

    if (!tlv_parse(data, size, (f_tlv_data_handler)&handle_enum_value_struct, &ctx))
        return 1;
    
    return verify_enum_value_struct(&ctx);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    explicit_bzero(&tmpContent, sizeof(tmpContent_t));
    explicit_bzero(&txContext, sizeof(txContext_t));
    explicit_bzero(&txContent, sizeof(txContent_t));
    explicit_bzero(&tmpCtx, sizeof(tmpCtx_t));
    explicit_bzero(&strings, sizeof(strings_t));
    explicit_bzero(&G_io_apdu_buffer, 260);

    txContext.content = &txContent;
    txContext.sha3 = &sha3;

    if (size < 1)
        return 0;
    switch (data[0])
    {
    case 0:
        fuzzGenericParserFieldCmd(++data, --size);
        break;
    case 1:
        fuzzGenericParserTxInfoCmd(++data, --size);
        break;
    case 2:
        fuzzGenericParserEnumCmd(++data, --size);
        break;
    default:
        return 0;
    }

    return 0;
}
