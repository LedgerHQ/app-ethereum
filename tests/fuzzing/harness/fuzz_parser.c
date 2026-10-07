/*
 * fuzz_parser: the stateful parsers, called at their own entry points.
 *
 * The generic tx parser, EIP-712 and the calldata store are all reachable from
 * an INS, but only after a multi-APDU handshake that a single dispatched
 * command cannot set up, so fuzz_app leaves most of them unreached. This target
 * skips the APDU layer and hands each parser the payload directly.
 *
 * Control byte 1 picks the parser; everything after the control bytes is
 * payload, consumed in order.
 */
#include <string.h>

#include "mocks.h"
#include "fuzz_input.h"

#include "shared_context.h"
#include "buffer.h"
#include "gtp_field.h"
#include "gtp_tx_info.h"
#include "enum_value.h"
#include "eip712_v1_commands.h"
#include "eip712_v1_context.h"
#include "calldata.h"
#include "mem_utils.h"  // app_mem_init()

#include "fuzz_harness.h"

enum {
    PARSER_GTP_FIELD,
    PARSER_GTP_TX_INFO,
    PARSER_ENUM_VALUE,
    PARSER_EIP712,
    PARSER_CALLDATA,
    PARSER_COUNT,
};

#define PARSER_ROW(idx) {.ins = (idx), .flags = FUZZ_CMD_HAS_DATA}
const fuzz_command_spec_t fuzz_commands[] = {
    PARSER_ROW(PARSER_GTP_FIELD),
    PARSER_ROW(PARSER_GTP_TX_INFO),
    PARSER_ROW(PARSER_ENUM_VALUE),
    PARSER_ROW(PARSER_EIP712),
    PARSER_ROW(PARSER_CALLDATA),
};
FUZZ_COMMAND_COUNT();
_Static_assert(ARRAYLEN(fuzz_commands) == PARSER_COUNT, "parser tables out of sync");

extern const chain_config_t g_fuzz_chain_config;  // mock/app_globals.c

void fuzz_app_reset(void) {
    reset_app_context();
    app_mem_init();

    g_caller_app = NULL;
    // Absolution zeroes g_chain_config (zero-symbols); production keeps it set.
    g_chain_config = &g_fuzz_chain_config;
}

static void run_gtp_field(buffer_t *buf) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};

    if (!handle_field_struct(buf, &ctx) || !verify_field_struct(&ctx)) {
        cleanup_field(&field);
        return;
    }
    // format_field() cleans up its constraints on both paths.
    (void) format_field(&field, 0);
}

static void run_gtp_tx_info(buffer_t *buf) {
    s_tx_info tx_info = {0};
    s_tx_info_ctx ctx = {.tx_info = &tx_info};

    cx_sha256_init(&ctx.struct_hash);
    if (handle_tx_info_struct(buf, &ctx)) {
        (void) verify_tx_info_struct(&ctx);
    }
}

static void run_enum_value(buffer_t *buf) {
    s_enum_value_ctx ctx = {0};

    cx_sha256_init(&ctx.hash_ctx);
    if (handle_enum_value_tlv_payload(buf, &ctx)) {
        (void) verify_enum_value_struct(&ctx);
    }
}

/*
 * The EIP-712 command set, replayed as a sequence.
 *
 * handle_eip712_v1_filtering() returns before entering eip712_v1_filtering.c
 * unless the mode is already FULL, and only a prior P2_FILT_ACTIVATE sets it;
 * a type is likewise built by several struct-def calls. One call per input can
 * reach neither. The fuzzer picks the op, P1, P2 and payload of every step, and
 * how many steps there are.
 */
static void run_eip712(fuzz_cursor_t *cur) {
    if (!eip712_v1_context_init()) {
        return;
    }
    while (cur->left > 0) {
        uint8_t op = fuzz_take_u8(cur);
        if (op == 0) {
            break;
        }
        uint8_t p1 = fuzz_take_u8(cur);
        uint8_t p2 = fuzz_take_u8(cur);
        uint8_t len;
        const uint8_t *payload = fuzz_take_slice(cur, &len);

        switch (op % 4) {
            case 0:
                handle_eip712_v1_struct_def(p2, payload, len);
                break;
            case 1:
                handle_eip712_v1_struct_impl(p1, p2, payload, len);
                break;
            case 2:
                handle_eip712_v1_filtering(p1, p2, payload, len);
                break;
            default:
                handle_eip712_v1_sign(payload, len);
                break;
        }
    }
    eip712_v1_context_deinit();
}

/* Append/read program over one calldata store. The store is local so it cannot
 * outlive the heap that app_mem_init() resets between iterations. */
static void run_calldata(fuzz_cursor_t *cur) {
    s_calldata *calldata = calldata_init(CALLDATA_SELECTOR_SIZE + cur->left, NULL);

    if (calldata == NULL) {
        return;
    }
    while (cur->left > 0) {
        const uint8_t *chunk;
        uint8_t len;

        if (fuzz_take_u8(cur) & 1) {
            chunk = fuzz_take_slice(cur, &len);
            calldata_append(calldata, chunk, len);
        } else {
            (void) calldata_get_chunk(calldata, fuzz_take_u8(cur));
        }
    }
    calldata_delete(calldata);
}

/* The GTP and enum handlers each take one complete TLV payload. */
static void run_tlv_step(uint8_t parser, fuzz_cursor_t *cur) {
    uint8_t len;
    const uint8_t *payload = fuzz_take_slice(cur, &len);
    buffer_t buf = {.ptr = (uint8_t *) payload, .size = len, .offset = 0};

    switch (parser) {
        case PARSER_GTP_FIELD:
            run_gtp_field(&buf);
            break;
        case PARSER_GTP_TX_INFO:
            run_gtp_tx_info(&buf);
            break;
        default:
            run_enum_value(&buf);
            break;
    }
}

/*
 * One input drives a sequence of parser calls: a field table is built by
 * several field structs after a tx info, the same way the dispatcher feeds them.
 */
void fuzz_app_dispatch(void *cmd_v) {
    const command_t *cmd = (const command_t *) cmd_v;
    fuzz_cursor_t cur = {.ptr = fuzz_tail_ptr, .left = fuzz_tail_len};
    uint8_t parser = cmd->ins % PARSER_COUNT;

    for (;;) {
        switch (parser) {
            case PARSER_EIP712:
                run_eip712(&cur);
                break;
            case PARSER_CALLDATA:
                run_calldata(&cur);
                break;
            default:
                run_tlv_step(parser, &cur);
                break;
        }
        if (cur.left == 0 || fuzz_take_u8(&cur) == 0) {
            break;
        }
        parser = fuzz_take_u8(&cur) % PARSER_COUNT;
    }
}
