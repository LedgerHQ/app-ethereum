/*
 * fuzz_app: the full-app target. One fuzzer input becomes one APDU, parsed by
 * the real apdu_parser() and dispatched through handleApdu().
 *
 * One input drives a whole sequence of APDUs; see fuzz_app_dispatch() for why.
 * Adding a production INS means one row in ETH_FUZZ_COMMANDS plus a dictionary
 * entry in fuzz-manifest.toml.
 */
#include <string.h>

#include "mocks.h"
#include "tlv_mutator.h"
#include "fuzz_input.h"
#include "shared_context.h"
#include "apdu_constants.h"     // handleApdu()
#include "eip712_v1_context.h"  // eip712_v1_context_deinit()
#include "tx_ctx.h"             // gcs_cleanup()
#include "trusted_name.h"       // trusted_name_cleanup()
#include "enum_value.h"         // enum_value_cleanup()
#include "proxy_info.h"         // proxy_cleanup()
#include "sign_message.h"       // message_cleanup()
#include "map_entry.h"          // map_entry_cleanup()
#include "tlv_apdu.h"           // tlv_from_apdu()
#include "app_mem_utils.h"      // APP_MEM_FREE_AND_NULL()
#include "mem_utils.h"          // app_mem_init()

/* This app supplies a sequence-aware TLV mutator (below). */
#define FUZZ_APP_CUSTOM_MUTATOR
#include "fuzz_harness.h"

extern const chain_config_t g_fuzz_chain_config;  // mock/app_globals.c

/* ── TLV grammars: accepted (tag, min_len, max_len) per command ───────────── */

static const tlv_tag_info_t TAGS_TRUSTED_NAME[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x10, 3, 3},
    {0x12, 1, 4},
    {0x13, 1, 2},
    {0x14, 1, 2},
    {0x20, 1, 30},
    {0x21, 1, 4},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x70, 1, 1},
    {0x71, 1, 1},
    {0x72, 1, 32},
    {0x74, 20, 20},
    {0x75, 9, 41},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_ENUM_VALUE[] = {
    {0x00, 1, 1},
    {0x01, 1, 8},
    {0x02, 20, 20},
    {0x03, 0, 4},
    {0x04, 1, 1},
    {0x05, 1, 1},
    {0x06, 1, 20},
    {0xff, 8, 72},
};

static const tlv_tag_info_t TAGS_GATING[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x40, 0, 28},
    {0x82, 0, 100},
    {0x83, 0, 30},
    {0x84, 1, 1},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_TX_SIMULATION[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x27, 0, 32},
    {0x28, 0, 32},
    {0x80, 1, 1},
    {0x81, 1, 1},
    {0x82, 0, 25},
    {0x83, 0, 30},
    {0x84, 1, 1},
    {0x85, 1, 64},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_PROXY_INFO[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x12, 1, 4},
    {0x22, 20, 20},
    {0x23, 1, 8},
    {0x41, 0, 4},
    {0x42, 20, 20},
    {0x43, 1, 1},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_NETWORK_INFO[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x51, 1, 1},
    {0x23, 1, 8},
    {0x52, 0, 31},
    {0x24, 0, 50},
    {0x53, 0, 32},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_SAFE_DESCRIPTOR[] = {
    {0x01, 1, 1},
    {0x02, 1, 1},
    {0x12, 1, 4},
    {0x22, 20, 20},
    {0xa0, 1, 2},
    {0xa1, 1, 2},
    {0xa2, 1, 1},
    {0x15, 70, 72},
};

static const tlv_tag_info_t TAGS_GTP_TX_INFO[] = {
    {0x00, 1, 1},
    {0x01, 1, 8},
    {0x02, 20, 20},
    {0x03, 0, 4},
    {0x04, 0, 32},
    {0x05, 0, 31},
    {0x06, 0, 23},
    {0x07, 0, 31},
    {0x08, 0, 27},
    {0x09, 0, 31},
    {0x0a, 0, 4},
    {0xff, 0, 72},
};

static const tlv_tag_info_t TAGS_GTP_FIELD[] = {
    {0x00, 1, 1},
    {0x01, 1, 20},
    {0x02, 1, 1},
    {0x03, 1, 64},
    {0x04, 1, 1},
    {0x05, 1, 64},
};

static const tlv_tag_info_t TAGS_AUTH_7702[] = {
    {0x00, 1, 1},
    {0x01, 20, 20},
    {0x02, 1, 8},
    {0x03, 1, 8},
};

static const tlv_tag_info_t TAGS_MAP_ENTRY[] = {
    {0x00, 1, 1},
    {0x01, 1, 8},
    {0x02, 20, 20},
    {0x03, 4, 4},
    {0x04, 1, 1},
    {0x05, 1, 32},
    {0x06, 1, 32},
    {0xff, 70, 72},
};

#define _TLV_INLINE_NONE            {.tags_info = NULL, .num_tags = 0}
#define _TLV_INLINE_TRUSTED_NAME    TLV_CFG(TAGS_TRUSTED_NAME)
#define _TLV_INLINE_ENUM_VALUE      TLV_CFG(TAGS_ENUM_VALUE)
#define _TLV_INLINE_GATING          TLV_CFG(TAGS_GATING)
#define _TLV_INLINE_TX_SIMULATION   TLV_CFG(TAGS_TX_SIMULATION)
#define _TLV_INLINE_PROXY_INFO      TLV_CFG(TAGS_PROXY_INFO)
#define _TLV_INLINE_NETWORK_INFO    TLV_CFG(TAGS_NETWORK_INFO)
#define _TLV_INLINE_SAFE_DESCRIPTOR TLV_CFG(TAGS_SAFE_DESCRIPTOR)
#define _TLV_INLINE_GTP_TX_INFO     TLV_CFG(TAGS_GTP_TX_INFO)
#define _TLV_INLINE_GTP_FIELD       TLV_CFG(TAGS_GTP_FIELD)
#define _TLV_INLINE_AUTH_7702       TLV_CFG(TAGS_AUTH_7702)
#define _TLV_INLINE_MAP_ENTRY       TLV_CFG(TAGS_MAP_ENTRY)

/* One row per fuzzable APDU: INS symbol, P1 max, P2 max, flags, TLV grammar.
 * (P*_max = 0 means the full [0,255] range; the harness clamps to max+1.) */
// clang-format off
#define ETH_FUZZ_COMMANDS(X)                                                            \
    X(INS_GET_PUBLIC_KEY,                  1,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_SIGN,                            0x80, 0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_GET_APP_CONFIGURATION,           0,    0,    0,                 NONE)          \
    X(INS_SIGN_PERSONAL_MESSAGE,           0x80, 0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_PROVIDE_ERC20_TOKEN_INFORMATION, 0,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_SIGN_EIP_712_MESSAGE,            1,    1,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_GET_ETH2_PUBLIC_KEY,             1,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_SET_ETH2_WITHDRAWAL_INDEX,       0,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_SET_EXTERNAL_PLUGIN,             0,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_PROVIDE_NFT_INFORMATION,         0,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_SET_PLUGIN,                      0,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_PERFORM_PRIVACY_OPERATION,       1,    0,    FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_EIP712_STRUCT_DEF,               0,    0xFF, FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_EIP712_STRUCT_IMPL,              1,    0xFF, FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_EIP712_FILTERING,                1,    0xFF, FUZZ_CMD_HAS_DATA, NONE)          \
    X(INS_GET_CHALLENGE,                   0,    0,    0,                 NONE)          \
    X(INS_PROVIDE_TRUSTED_NAME,            1,    0,    FUZZ_CMD_HAS_DATA, TRUSTED_NAME)  \
    X(INS_PROVIDE_ENUM_VALUE,              1,    0,    FUZZ_CMD_HAS_DATA, ENUM_VALUE)    \
    X(INS_GTP_TRANSACTION_INFO,            1,    0,    FUZZ_CMD_HAS_DATA, GTP_TX_INFO)   \
    X(INS_GTP_FIELD,                       1,    0,    FUZZ_CMD_HAS_DATA, GTP_FIELD)     \
    X(INS_PROVIDE_PROXY_INFO,              1,    0,    FUZZ_CMD_HAS_DATA, PROXY_INFO)    \
    X(INS_PROVIDE_NETWORK_CONFIGURATION,   1,    0,    FUZZ_CMD_HAS_DATA, NETWORK_INFO)  \
    X(INS_PROVIDE_TX_SIMULATION,           1,    0,    FUZZ_CMD_HAS_DATA, TX_SIMULATION) \
    X(INS_SIGN_EIP7702_AUTHORIZATION,      1,    0,    FUZZ_CMD_HAS_DATA, AUTH_7702)     \
    X(INS_PROVIDE_SAFE_ACCOUNT,            1,    1,    FUZZ_CMD_HAS_DATA, SAFE_DESCRIPTOR)\
    X(INS_PROVIDE_GATING,                  1,    0,    FUZZ_CMD_HAS_DATA, GATING)        \
    X(INS_PROVIDE_MAP_ENTRY,               1,    0,    FUZZ_CMD_HAS_DATA, MAP_ENTRY)
// clang-format on

/* Parameter names are prefixed (c_*) so they never collide with the struct
 * field designators (.ins, .p1_max, ...) during macro expansion. */
#define _CMD_SPEC(c_ins, c_p1, c_p2, c_flags, c_tlv) \
    {.cla = CLA, .ins = (c_ins), .p1_max = (c_p1), .p2_max = (c_p2), .flags = (c_flags)},
const fuzz_command_spec_t fuzz_commands[] = {ETH_FUZZ_COMMANDS(_CMD_SPEC)};
FUZZ_COMMAND_COUNT();
#undef _CMD_SPEC

#define _CMD_TLV(c_ins, c_p1, c_p2, c_flags, c_tlv) _TLV_INLINE_##c_tlv,
static const tlv_fuzz_config_t k_command_tlv_configs[] = {ETH_FUZZ_COMMANDS(_CMD_TLV)};
#undef _CMD_TLV

/*
 * Grammar-aware mutation of the sequence's last step.
 *
 * The TLV commands only parse a descriptor the fuzzer got structurally right,
 * so a mutator that keeps one valid is worth a lot; but it has to leave the
 * steps before it alone, or every mutation destroys the sequence that reached
 * this point. Working on the last step means a size change disturbs nothing.
 *
 * Only the framing is preserved. Tag order, values and lengths within the
 * declared bounds all still come from the fuzzer, and the other half of the
 * mutations (below) are generic, which is what produces malformed framing.
 */
static size_t eth_mutate_last_step(uint8_t *input,
                                   size_t size,
                                   size_t max_size,
                                   unsigned int seed) {
    size_t step = FUZZ_CTRL_LEN; /* offset of the current step's length byte */
    size_t cmd = input[1] % fuzz_n_commands;
    size_t last_step = step, last_cmd = cmd;

    while (step < size) {
        size_t next = step + 1 + input[step];
        last_step = step;
        last_cmd = cmd;
        /* stop byte, then ins/p1/p2 for the following step */
        if ((next + 4) >= size || input[next] == 0) {
            break;
        }
        cmd = input[next + 1] % fuzz_n_commands;
        step = next + 4;
    }

    const tlv_fuzz_config_t *cfg = &k_command_tlv_configs[last_cmd];
    size_t body = last_step + 1 + 2; /* step length byte, then the TLV length header */

    if (cfg->num_tags == 0 || (body + 1) >= max_size) {
        return LLVMFuzzerMutate(input, size, max_size);
    }

    current_tlv_fuzz_config = *cfg;
    size_t room = max_size - body;
    if (room > UINT8_MAX - 2) {
        room = UINT8_MAX - 2; /* the step still has to fit one APDU payload */
    }
    size_t tlv = tlv_custom_mutate(input + body, (size > body) ? size - body : 0, room, seed);

    input[last_step] = (uint8_t) (2 + tlv);
    input[last_step + 1] = (uint8_t) (tlv >> 8);
    input[last_step + 2] = (uint8_t) (tlv & 0xFF);
    return body + tlv;
}

size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t max_size, unsigned int seed) {
    if ((seed & 1U) == 0) {
        return fuzz_mutate_input_with(data, size, max_size, seed >> 1, eth_mutate_last_step);
    }
    return fuzz_custom_mutator(data, size, max_size, seed);
}

/* Not reset_app_context(): that memsets tmpCtx, tmpContent and txContext, which
 * is exactly the state Absolution restored from the prefix. Free the per-command
 * allocations instead and leave the sampled globals standing. */
void fuzz_app_reset(void) {
    /* Several cleanups below reset appState to IDLE as a side effect; keep the
     * fuzzed value so the invariant stays the single source of protocol state. */
    uint8_t restored_app_state = appState;

    g_caller_app = NULL;
    // Absolution zeroes g_chain_config (zero-symbols); production keeps it set.
    g_chain_config = &g_fuzz_chain_config;

    /* Drop any partial multi-chunk TLV from the previous iteration (NULL payload
     * makes tlv_from_apdu() free its accumulator via the public API). */
    (void) tlv_from_apdu(false, 0, NULL, NULL);
    eip712_v1_context_deinit();
    gcs_cleanup();
    trusted_name_cleanup();
    enum_value_cleanup();
    proxy_cleanup();
    map_entry_cleanup();
    message_cleanup();

    extern cx_sha3_t *g_msg_hash_ctx;
    APP_MEM_FREE_AND_NULL((void **) &g_msg_hash_ctx);

    app_mem_init();

    /* Re-point txContext AFTER the cleanups: eip712_v1_context_deinit() calls
     * reset_app_context(), which memsets txContext, so a SIGNING_TX prefix needs
     * its content re-wired or format_param_enum() derefs a NULL txContent. */
    txContext.sha3 = &global_sha3;
    (void) cx_keccak_init_no_throw(&global_sha3, 256);
    txContext.workBuffer = NULL;
    txContext.content = &tmpContent.txContent;

    appState = restored_app_state;
}

/** Re-encodes one command as a real APDU and runs the parser production uses. */
static void dispatch_one(uint8_t cla,
                         uint8_t ins,
                         uint8_t p1,
                         uint8_t p2,
                         const uint8_t *payload,
                         uint8_t lc) {
    uint8_t apdu[5 + UINT8_MAX] = {0};
    command_t parsed = {0};
    uint32_t tx = 0;

    apdu[0] = cla;
    apdu[1] = ins;
    apdu[2] = p1;
    apdu[3] = p2;
    apdu[4] = lc;
    if (lc > 0 && payload != NULL) {
        memcpy(apdu + 5, payload, lc);
    }

    if (!apdu_parser(&parsed, apdu, 5 + lc)) {
        return;
    }
    uint16_t sw = handleApdu(&parsed, &tx);
    if ((sw != SWO_NO_RESPONSE) && (sw != SWO_SUCCESS) && (sw != SWO_COMMAND_CODE_NOT_SUPPORTED)) {
        reset_app_context();
    }
}

/*
 * One input drives a sequence of APDUs, not a single one.
 *
 * Twelve commands accumulate their TLV descriptor across APDUs through
 * tlv_from_apdu(), which only runs the payload handler once the descriptor is
 * complete. An APDU carries at most 253 bytes of TLV after the two-byte length
 * header, so a single dispatch can never finish a descriptor that carries a
 * signature -- it allocates the accumulator and the next iteration drops it.
 *
 * The fuzzer picks every field of every step, including how many steps there
 * are: nothing here supplies content, it only stops discarding the rest of the
 * input after the first command.
 */
void fuzz_app_dispatch(void *cmd_v) {
    const command_t *cmd = (const command_t *) cmd_v;
    const fuzz_command_spec_t *spec = NULL;
    fuzz_cursor_t cur = {.ptr = fuzz_tail_ptr, .left = fuzz_tail_len};
    uint8_t p1 = cmd->p1;
    uint8_t p2 = cmd->p2;
    uint8_t cla = cmd->cla;
    uint8_t ins = cmd->ins;

    for (;;) {
        uint8_t lc;
        const uint8_t *payload = fuzz_take_slice(&cur, &lc);

        dispatch_one(cla, ins, p1, p2, payload, lc);

        // A zero byte ends the sequence; so does running out of input.
        if (cur.left == 0 || fuzz_take_u8(&cur) == 0) {
            break;
        }
        spec = &fuzz_commands[fuzz_take_u8(&cur) % fuzz_n_commands];
        cla = spec->cla;
        ins = spec->ins;
        p1 = fuzz_clamp_p(fuzz_take_u8(&cur), spec->p1_max);
        p2 = fuzz_clamp_p(fuzz_take_u8(&cur), spec->p2_max);
    }
}
