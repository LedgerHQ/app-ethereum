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
#include "gcs_field.h"
#include "operation_info.h"
#include "enum_value.h"
#include "eip712_v1_commands.h"
#include "eip712_v1_context.h"
#include "eip712_session.h"
#include "eip712_v2_schema.h"
#include "eip712_v2_values.h"
#include "eip712_v2_message_info.h"
#include "eip712_v2_path.h"
#include "typed_data.h"
#include "operation_ctx.h"  // operation_ctx_has_message()
#include "calldata.h"
#include "mem_utils.h"  // app_mem_init()

#include "fuzz_harness.h"

enum {
    PARSER_GCS_FIELD,
    PARSER_OPERATION_INFO,
    PARSER_ENUM_VALUE,
    PARSER_EIP712,
    PARSER_CALLDATA,
    PARSER_EIP712_V2_SCHEMA,
    PARSER_EIP712_V2_VALUES,
    PARSER_EIP712_V2_MESSAGE_INFO,
    PARSER_EIP712_PATH,
    PARSER_COUNT,
};

#define PARSER_ROW(idx) {.ins = (idx), .flags = FUZZ_CMD_HAS_DATA}
const fuzz_command_spec_t fuzz_commands[] = {
    PARSER_ROW(PARSER_GCS_FIELD),
    PARSER_ROW(PARSER_OPERATION_INFO),
    PARSER_ROW(PARSER_ENUM_VALUE),
    PARSER_ROW(PARSER_EIP712),
    PARSER_ROW(PARSER_CALLDATA),
    PARSER_ROW(PARSER_EIP712_V2_SCHEMA),
    PARSER_ROW(PARSER_EIP712_V2_VALUES),
    PARSER_ROW(PARSER_EIP712_V2_MESSAGE_INFO),
    PARSER_ROW(PARSER_EIP712_PATH),
};
FUZZ_COMMAND_COUNT();
_Static_assert(ARRAYLEN(fuzz_commands) == PARSER_COUNT, "parser tables out of sync");

extern const chain_config_t g_fuzz_chain_config;  // mock/app_globals.c

void fuzz_app_reset(void) {
    fuzz_pending_reset = false;
    reset_app_context();
    app_mem_init();

    g_caller_app = NULL;
    // Absolution zeroes g_chain_config (zero-symbols); production keeps it set.
    g_chain_config = &g_fuzz_chain_config;
}

static void run_gcs_field(buffer_t *buf) {
    s_field field = {0};
    s_field_ctx ctx = {.field = &field};

    if (!handle_field_struct(buf, &ctx) || !verify_field_struct(&ctx)) {
        cleanup_field(&field);
        return;
    }
    // format_field() cleans up its constraints on both paths.
    (void) format_field(&field, 0);
}

/* The same struct is a TRANSACTION_INFO and an EIP712_MESSAGE_INFO depending on the command it
 * comes in, so every input goes through both. */
static void run_operation_info(buffer_t *buf) {
    const e_operation_info_context contexts[] = {OPERATION_INFO_CTX_SIGN_TX,
                                                 OPERATION_INFO_CTX_EIP712};

    for (size_t i = 0; i < ARRAYLEN(contexts); i++) {
        buffer_t payload = *buf;
        s_operation_info *info = new_operation_info(&payload, contexts[i]);

        if (info != NULL) {
            delete_operation_info(info);
        }
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

        uint16_t sw;
        switch (op % 4) {
            case 0:
                sw = handle_eip712_v1_struct_def(p2, payload, len);
                break;
            case 1:
                sw = handle_eip712_v1_struct_impl(p1, p2, payload, len);
                break;
            case 2:
                sw = handle_eip712_v1_filtering(p1, p2, payload, len);
                break;
            default:
                sw = handle_eip712_v1_sign(payload, len);
                break;
        }
        // Same reset as main.c before the next command; the context is rebuilt lazily.
        if (fuzz_pending_reset || ((sw != SWO_NO_RESPONSE) && (sw != SWO_SUCCESS) &&
                                   (sw != SWO_COMMAND_CODE_NOT_SUPPORTED))) {
            fuzz_pending_reset = false;
            reset_app_context();
        }
    }
    eip712_v1_context_deinit();
    td_deinit();
    eip712_session_close();
}

/* Append/read program over one calldata store. The store is local so it cannot
 * outlive the heap that app_mem_init() resets between iterations. */
static void run_calldata(fuzz_cursor_t *cur) {
    size_t size;
    const uint8_t *blob = fuzz_take_slice16(cur, &size);
    s_calldata *calldata = calldata_init(size, NULL);
    size_t appended = 0;

    if (calldata == NULL) {
        return;
    }
    while (cur->left > 0) {
        uint8_t op = fuzz_take_u8(cur);
        const uint8_t *chunk;
        uint8_t len;

        if (op & 1) {
            // Next piece of the blob, so the store can complete and reads succeed.
            size_t piece = fuzz_take_u8(cur);

            if (piece > size - appended) {
                piece = size - appended;
            }
            if ((piece > 0) && calldata_append(calldata, blob + appended, piece)) {
                appended += piece;
            }
        } else if (op & 2) {
            // Bytes from outside the blob, which may overrun the declared size.
            chunk = fuzz_take_slice(cur, &len);
            calldata_append(calldata, chunk, len);
        } else {
            (void) calldata_get_chunk(calldata, fuzz_take_u8(cur));
        }
    }
    calldata_delete(calldata);
}

/*
 * An EIP-712 V2 message, built by one TLV payload per step: its schema, then its values, then its
 * message info. Each step checks what its command checks, so the steps after it meet the state a
 * real flow leaves; once a message info is in, the FIELD steps read their EIP712_PATH against the
 * message.
 */

// The schema opens a new flow, as its first chunk does after the app was reset
static void run_eip712_v2_schema(buffer_t *buf) {
    reset_app_context();
    if (!eip712_session_open(EIP712_SESSION_V2) || !td_init()) {
        return;
    }
    (void) handle_eip712_v2_schema_struct(buf);
}

// The values fill a complete schema, once
static void run_eip712_v2_values(buffer_t *buf) {
    if ((eip712_session_get() != EIP712_SESSION_V2) || !td_has_schema() || td_has_domain() ||
        td_has_message()) {
        return;
    }
    (void) handle_eip712_v2_values_struct(buf);
}

// The info is bound to a message that has its values, and a message has one
static void run_eip712_v2_message_info(buffer_t *buf) {
    if ((eip712_session_get() != EIP712_SESSION_V2) || !td_has_domain() || !td_has_message() ||
        operation_ctx_has_message()) {
        return;
    }
    (void) handle_eip712_v2_message_info_struct(buf);
}

// A path is resolved against the message, when the steps before built one
static void run_eip712_path(buffer_t *buf) {
    s_eip712_path path;
    s_eip712_path_context ctx = {.path = &path};
    s_parsed_value_collection collection;

    if (handle_eip712_path_struct(buf, &ctx)) {
        (void) eip712_path_get(&path, &collection);
    }
}

/* The GCS, enum and EIP-712 V2 handlers each take one complete TLV payload. */
static void run_tlv_step(uint8_t parser, fuzz_cursor_t *cur) {
    size_t len;
    const uint8_t *payload = fuzz_take_slice16(cur, &len);
    buffer_t buf = {.ptr = (uint8_t *) payload, .size = len, .offset = 0};

    switch (parser) {
        case PARSER_GCS_FIELD:
            run_gcs_field(&buf);
            break;
        case PARSER_OPERATION_INFO:
            run_operation_info(&buf);
            break;
        case PARSER_ENUM_VALUE:
            run_enum_value(&buf);
            break;
        case PARSER_EIP712_V2_SCHEMA:
            run_eip712_v2_schema(&buf);
            break;
        case PARSER_EIP712_V2_VALUES:
            run_eip712_v2_values(&buf);
            break;
        case PARSER_EIP712_V2_MESSAGE_INFO:
            run_eip712_v2_message_info(&buf);
            break;
        case PARSER_EIP712_PATH:
            run_eip712_path(&buf);
            break;
        default:
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
