/*
 * Dispatcher runtime: per-iteration reset + harness-side state bootstraps.
 *
 * Linked only by the SDK dispatcher binary (`fuzz_app`); the per-feature
 * harnesses use mock/app_globals.c directly. The functions here implement
 * two contracts that the SDK glue and the adapter both rely on:
 *
 * 1. Per-iteration reset (`fuzz_reset_runtime_state`, called from
 *    `fuzz_app_reset()` in fuzz_dispatcher.c before every libFuzzer
 *    iteration). It rebuilds the chain config, points `txContext.content`
 *    at the shared `tmpContent.txContent`, calls `fuzz_non_apdu_reset()`
 *    to re-pin the swap callback globals to harness-owned scratch, frees
 *    any heap-owned contexts that survived the previous iteration, then
 *    calls `app_mem_init()`.
 *
 *    Order constraint: `fuzz_non_apdu_reset()` MUST run before
 *    `app_mem_init()`. The reset only rewrites file-scope pointers to
 *    static buffers (it does not allocate from the app heap), so it is
 *    safe to call before init; running it after init would leave the
 *    swap globals pointing into a heap region that has just been zeroed
 *    by the harness and re-handed to the allocator, racing with the next
 *    iteration's bootstrap allocations.
 *
 * 2. Bootstrap helpers (`fuzz_bootstrap_gtp_ctx`, `fuzz_bootstrap_eip712_ctx`,
 *    `fuzz_bootstrap_safe_ctx`, `fuzz_bootstrap_enum_entries`). Called by
 *    the adapter (fuzz_apdu_adapter.c) AFTER reset, so they allocate from
 *    a freshly-initialised heap. They simulate the multi-APDU prerequisite
 *    state production code normally builds via INS_SIGN /
 *    INS_EIP712_STRUCT_DEF / INS_PROVIDE_SAFE_ACCOUNT so that a single-INS
 *    fuzz iteration can still exercise downstream handlers (INS_GTP_FIELD,
 *    INS_EIP712_STRUCT_IMPL, signer descriptors).
 */

#include "fuzz_runtime.h"

#include <string.h>

#include "mocks.h"
#include "shared_context.h"
#include "context_712.h"
#include "tx_ctx.h"
#include "trusted_name.h"
#include "enum_value.h"
#include "proxy_info.h"
#include "sign_message.h"
#include "map_entry.h"
#include "app_mem_utils.h"
#include "tlv_apdu.h"
#include "mem_utils.h"
#include "eth_swap_utils.h"
#include "swap_utils.h"
#include "fuzz_non_apdu.h"
#include "calldata.h"
#include "safe_descriptor.h"
#include "signer_descriptor.h"
#include "gtp_tx_info.h"
#include "gtp_field_table.h"
#include "typed_data.h"
#include "path.h"
#include "ui_logic.h"

static chain_config_t k_fuzz_chain_config;
static const uint8_t k_fuzz_selector[CALLDATA_SELECTOR_SIZE] = {0xAA, 0xBB, 0xCC, 0xDD};

/* Harness-side state bootstraps.
 *
 * Called from the dispatcher (not from reset) so they run AFTER reset has
 * cleaned up prior-iteration state and AFTER app_mem_init() has reset the
 * heap. They simulate the multi-APDU prerequisite state production code
 * normally builds via INS_SIGN / INS_EIP712_STRUCT_DEF /
 * INS_PROVIDE_SAFE_ACCOUNT.
 *
 * Trade-off: the bootstrap skips the APDU-level parsing that normally
 * builds these structures. That parsing is already fuzzed by seeds that
 * target those INS directly. The bootstrap only provides the minimum
 * prerequisite state so dependent handlers (INS_GTP_FIELD,
 * INS_EIP712_STRUCT_IMPL, signer descriptors, …) can exercise their own
 * deep parsing and memory logic. */
void fuzz_bootstrap_gtp_ctx(void) {
    s_calldata *cd = calldata_init(64, k_fuzz_selector);
    if (cd == NULL) return;

    static const uint8_t calldata_body[64] = {0};
    calldata_append(cd, calldata_body, sizeof(calldata_body));

    g_parked_calldata = cd;
    uint64_t chain_id = APP_CHAIN_ID;
    uint8_t to[ADDRESS_LENGTH] = {0};
    if (!tx_ctx_init(cd, NULL, to, NULL, &chain_id)) {
        if (g_parked_calldata == cd) {
            calldata_delete(cd);
            g_parked_calldata = NULL;
        }
        return;
    }

    uint64_t match_chain = APP_CHAIN_ID;
    uint8_t match_addr[ADDRESS_LENGTH] = {0};
    if (!find_matching_tx_ctx(match_addr, k_fuzz_selector, &match_chain)) {
        return;
    }

    s_tx_info *tx_info;
    if (!APP_MEM_CALLOC((void **) &tx_info, sizeof(*tx_info))) return;
    tx_info->version = 1;
    tx_info->chain_id = APP_CHAIN_ID;
    memcpy(tx_info->selector, k_fuzz_selector, sizeof(tx_info->selector));
    strlcpy(tx_info->operation_type, "Swap", sizeof(tx_info->operation_type));
    memset(tx_info->fields_hash, 0xFF, sizeof(tx_info->fields_hash));
    if (!set_tx_info_into_tx_ctx(tx_info)) {
        APP_MEM_FREE(tx_info);
    }
}

/* EIP-712 bootstrap: initialise context, register a minimal type schema, set
 * both root types, and enable FULL filtering mode. The type schema mirrors
 * the original eip712_init_fuzz_types() but stays entirely in the harness
 * runtime. Field descriptors use the production binary format:
 *   TypeDesc [TypeSize] KeyNameLen KeyName */
void fuzz_bootstrap_eip712_ctx(void) {
    if (!eip712_context_init()) return;

    static const uint8_t domain_name[] = "EIP712Domain";
    static const uint8_t msg_name[] = "Mail";

    /* TypeDesc byte: bits [3:0]=type, bit6=has_size, bit7=is_array
     *   0x05 = TYPE_SOL_STRING
     *   0x42 = TYPE_SOL_UINT | TYPESIZE_MASK
     *   0x03 = TYPE_SOL_ADDRESS */
    static const uint8_t f_name[] = {0x05, 0x04, 'n', 'a', 'm', 'e'};
    static const uint8_t f_chain[] = {0x42, 0x20, 0x07, 'c', 'h', 'a', 'i', 'n', 'I', 'd'};
    static const uint8_t f_verify[] = {0x03,
                                       0x11,
                                       'v',
                                       'e',
                                       'r',
                                       'i',
                                       'f',
                                       'y',
                                       'i',
                                       'n',
                                       'g',
                                       'C',
                                       'o',
                                       'n',
                                       't',
                                       'r',
                                       'a',
                                       'c',
                                       't'};
    static const uint8_t f_value[] = {0x42, 0x20, 0x05, 'v', 'a', 'l', 'u', 'e'};
    static const uint8_t f_to[] = {0x03, 0x02, 't', 'o'};

    if (!set_struct_name(sizeof(domain_name) - 1, domain_name)) return;
    if (!set_struct_field(sizeof(f_name), f_name)) return;
    if (!set_struct_field(sizeof(f_chain), f_chain)) return;
    if (!set_struct_field(sizeof(f_verify), f_verify)) return;

    if (!set_struct_name(sizeof(msg_name) - 1, msg_name)) return;
    if (!set_struct_field(sizeof(f_value), f_value)) return;
    if (!set_struct_field(sizeof(f_to), f_to)) return;

    path_set_root((char *) domain_name, sizeof(domain_name) - 1);
    path_set_root((char *) msg_name, sizeof(msg_name) - 1);

    ui_712_set_filtering_mode(EIP712_FILTERING_FULL);
}

/* Enum value bootstrap: register synthetic enum entries so that
 * format_param_enum's get_matching_enum() can find matches.
 *
 * get_matching_enum() matches on (chain_id, contract_addr, selector, id,
 * value). The GTP bootstrap above sets chain_id=APP_CHAIN_ID,
 * destination=0..0, selector=0xAABBCCDD. We register a 4x4 grid of entries
 * (id 0..3, value 0..3) keyed to those values so enum lookup succeeds for
 * any fuzzed id/value in that range.
 *
 * Because g_enum_value_list is static in enum_value.c, we route the
 * registration through the production verify_enum_value_struct() path.
 * The TLV parser fills the s_enum_value_ctx, verify_fields checks the
 * received_tags bitfield, and verify_signature calls
 * check_signature_with_pubkey (mocked to always return true). */
static void fuzz_seed_one_enum(uint8_t id, uint8_t value, const char *name, uint8_t name_len) {
    /* TLV format: tag(1) + length(2 BE) + value(N).
     * Tags from ENUM_VALUE_TAGS: VERSION=0x00, CHAIN_ID=0x01,
     * CONTRACT_ADDR=0x02, SELECTOR=0x03, ID=0x04, VALUE=0x05, NAME=0x06,
     * SIGNATURE=0xFF. */
    uint8_t buf[128];
    size_t off = 0;

    buf[off++] = 0x00;
    buf[off++] = 0x00;
    buf[off++] = 0x01;
    buf[off++] = 0x01;

    buf[off++] = 0x01;
    buf[off++] = 0x00;
    buf[off++] = 0x08;
    buf[off++] = 0;
    buf[off++] = 0;
    buf[off++] = 0;
    buf[off++] = 0;
    buf[off++] = 0;
    buf[off++] = 0;
    buf[off++] = 0;
    buf[off++] = APP_CHAIN_ID;

    buf[off++] = 0x02;
    buf[off++] = 0x00;
    buf[off++] = ADDRESS_LENGTH;
    memset(buf + off, 0, ADDRESS_LENGTH);
    off += ADDRESS_LENGTH;

    buf[off++] = 0x03;
    buf[off++] = 0x00;
    buf[off++] = SELECTOR_SIZE;
    buf[off++] = 0xAA;
    buf[off++] = 0xBB;
    buf[off++] = 0xCC;
    buf[off++] = 0xDD;

    buf[off++] = 0x04;
    buf[off++] = 0x00;
    buf[off++] = 0x01;
    buf[off++] = id;

    buf[off++] = 0x05;
    buf[off++] = 0x00;
    buf[off++] = 0x01;
    buf[off++] = value;

    buf[off++] = 0x06;
    buf[off++] = 0x00;
    buf[off++] = name_len;
    memcpy(buf + off, name, name_len);
    off += name_len;

    static const uint8_t fake_sig[] = {0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01};
    buf[off++] = 0xFF;
    buf[off++] = 0x00;
    buf[off++] = (uint8_t) sizeof(fake_sig);
    memcpy(buf + off, fake_sig, sizeof(fake_sig));
    off += sizeof(fake_sig);

    s_enum_value_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    cx_sha256_init_no_throw(&ctx.hash_ctx);

    buffer_t payload = {.ptr = buf, .size = off, .offset = 0};
    if (!handle_enum_value_tlv_payload(&payload, &ctx)) return;
    verify_enum_value_struct(&ctx);
}

void fuzz_bootstrap_enum_entries(void) {
    static const char *names[] = {"None", "Low", "Mid", "High"};
    for (uint8_t id = 0; id < 4; ++id) {
        for (uint8_t val = 0; val < 4; ++val) {
            uint8_t idx = (id + val) % 4;
            fuzz_seed_one_enum(id, val, names[idx], (uint8_t) strlen(names[idx]));
        }
    }
}

void fuzz_bootstrap_safe_ctx(void) {
    if (SAFE_DESC != NULL) return;
    if (!APP_MEM_CALLOC((void **) &SAFE_DESC, sizeof(safe_descriptor_t))) return;
    SAFE_DESC->threshold = 1;
    SAFE_DESC->signers_count = 1;
    SAFE_DESC->role = ROLE_SIGNER;
}

void fuzz_reset_runtime_state(void) {
    memset(&k_fuzz_chain_config, 0, sizeof(k_fuzz_chain_config));
    k_fuzz_chain_config.chain_id = APP_CHAIN_ID;
    strlcpy(k_fuzz_chain_config.ticker, APP_TICKER, sizeof(k_fuzz_chain_config.ticker));

    g_chain_config = &k_fuzz_chain_config;
    g_caller_app = NULL;

    txContext.sha3 = &global_sha3;
    cx_keccak_init_no_throw(&global_sha3, 256);
    txContext.workBuffer = NULL;
    txContext.content = &tmpContent.txContent;

    fuzz_non_apdu_reset();

    tlv_cleanup();
    eip712_context_deinit();
    gcs_cleanup();
    trusted_name_cleanup();
    enum_value_cleanup();
    proxy_cleanup();
    map_entry_cleanup();
    message_cleanup();

    extern cx_sha3_t *g_msg_hash_ctx;
    APP_MEM_FREE_AND_NULL((void **) &g_msg_hash_ctx);

    app_mem_init();
}
