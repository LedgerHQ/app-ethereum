#include "mocks.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "bip32_utils.h"
#include "bip32.h"
#include "cx_errors.h"
#include "lcx_ecfp.h"
#include "tlv_library.h"
#include "cmd_safe_account.h"
#include "tx_ctx.h"
#include "ui_utils.h"
#include "mem_alloc.h"
#include "exceptions.h"
#include "os_task.h"

try_context_t fuzz_exit_jump_ctx = {0};
try_context_t *G_exception_context = &fuzz_exit_jump_ctx;

uint8_t fuzz_ctrl[FUZZ_CTRL_SIZE];
const uint8_t *fuzz_tail_ptr = NULL;
size_t fuzz_tail_len = 0;

try_context_t *try_context_get(void) {
    return G_exception_context;
}

try_context_t *try_context_set(try_context_t *context) {
    try_context_t *previous = G_exception_context;
    G_exception_context = context;
    return previous;
}

void __attribute__((noreturn))
os_sched_exit(bolos_task_status_t exit_code __attribute__((unused))) {
    longjmp(fuzz_exit_jump_ctx.jmp_buf, 1);
}

void __attribute__((noreturn)) os_lib_end(void) {
    longjmp(fuzz_exit_jump_ctx.jmp_buf, 1);
}

/* MSan does not see the volatile byte-wise stores that libc / lib_alloc use
 * to implement explicit_bzero (https://github.com/google/sanitizers/issues/1507),
 * so every stack array zeroed with explicit_bzero and every buffer returned
 * by mem_utils_calloc (which uses explicit_bzero internally) stays marked as
 * uninitialised. The taint then propagates through memcpy / struct reads and
 * triggers spurious use-of-uninitialized-value reports under -fsanitize=memory.
 *
 * Wrap explicit_bzero at link time with `-Wl,--wrap=explicit_bzero` and route
 * it through memset, which the MSan interceptor recognises and properly
 * unpoisons. The compiler barrier preserves the "do not optimise the clear
 * away" guarantee callers rely on for sensitive data. Production builds keep
 * the libc / SDK implementation untouched. */
void __wrap_explicit_bzero(void *s, size_t n) {
    if (s == NULL) return;
    memset(s, 0, n);
    __asm__ volatile("" ::: "memory");
}

mem_ctx_t __wrap_mem_init(void *heap_start, size_t heap_size) {
    (void) heap_size;
    return heap_start;
}

void *__wrap_mem_alloc(mem_ctx_t ctx, size_t nb_bytes) {
    (void) ctx;
    return malloc(nb_bytes);
}

void __wrap_mem_free(mem_ctx_t ctx, void *ptr) {
    (void) ctx;
    free(ptr);
}

cx_err_t cx_ecdomain_parameters_length(cx_curve_t cv, size_t *length) {
    (void) cv;
    *length = (size_t) 32;
    return CX_OK;
}

cx_err_t cx_ecdomain_size(cx_curve_t curve, size_t *length) {
    (void) curve;
    if (length) *length = 32;
    return CX_OK;
}

/* Keep cx_bn_t variables initialised (used by cx_ecdsa_verify_no_throw). */
cx_err_t cx_bn_alloc(cx_bn_t *x, size_t nbytes) {
    (void) nbytes;
    if (x) *x = 0;
    return CX_OK;
}

cx_err_t cx_bn_alloc_init(cx_bn_t *x, size_t nbytes, const uint8_t *value, size_t value_nbytes) {
    (void) nbytes;
    (void) value;
    (void) value_nbytes;
    if (x) *x = 0;
    return CX_OK;
}

cx_err_t cx_bn_cmp(const cx_bn_t a, const cx_bn_t b, int *diff) {
    (void) a;
    (void) b;
    if (diff) *diff = 0;
    return CX_OK;
}

/* Wraps SDK's cx_ecdsa_verify_no_throw to avoid MemorySanitizer false
 * positives (used through `-Wl,--wrap=cx_ecdsa_verify_no_throw`). */
bool __wrap_cx_ecdsa_verify_no_throw(const cx_ecfp_public_key_t *pukey,
                                     const uint8_t *hash,
                                     size_t hash_len,
                                     const uint8_t *sig,
                                     size_t sig_len) {
    (void) pukey;
    (void) hash;
    (void) hash_len;
    (void) sig;
    (void) sig_len;
    return true;
}

#ifdef PRINTF
#undef PRINTF
#endif
int PRINTF(const char *format, ...) {
    (void) format;
    return 0;
}

bool tlv_enforce_u8_value(const tlv_data_t *data, uint8_t expected) {
    uint8_t value = 0;

    if (data == NULL || !get_uint8_t_from_tlv_data(data, &value)) {
        return false;
    }
    return value == expected;
}

bool is_zeroes_buffer(const void *buf, size_t len) {
    const uint8_t *bytes = (const uint8_t *) buf;

    for (size_t i = 0; i < len; i++) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

void os_explicit_zero_BSS_segment(void) {
}

/* ── Stubs for symbols from excluded main.c ─────────────────────────── */

uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    (void) sw;
    (void) tx;
    (void) reset;
    (void) idle;
    return 0;
}

void app_main(void) {
}

void app_quit(void) {
}

void reset_app_context(void) {
    gcs_cleanup();
    clear_safe_account();
    ui_all_cleanup();
}

/*
 * Real parseBip32 implementation (from src/main.c).
 *
 * The previous stub capped path_components at 10 and ignored bip32->length /
 * bip32->path, masking edge cases in the real parser. This version reproduces
 * the production logic so the fuzzer exercises the same validation and
 * bip32_path_read call that runs on-device.
 */
const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    if (*dataLength < 1) {
        return NULL;
    }

    bip32->length = *dataBuffer;

    dataBuffer++;
    (*dataLength)--;

    if (*dataLength < sizeof(uint32_t) * (bip32->length)) {
        return NULL;
    }

    if (bip32_path_read(dataBuffer, (size_t) dataLength, bip32->path, (size_t) bip32->length) ==
        false) {
        return NULL;
    }
    dataBuffer += bip32->length * sizeof(uint32_t);
    *dataLength -= bip32->length * sizeof(uint32_t);

    return dataBuffer;
}

void coin_main(void *args) {
    (void) args;
}
void library_main(void *args) {
    (void) args;
}
void clone_main(void *args) {
    (void) args;
}
int ethereum_main(void *args) {
    (void) args;
    return 0;
}

/* ── Stubs for symbols from excluded network_icons.c ────────────────── */

#include "nbgl_types.h"
#include "caller_app.h"

const nbgl_icon_details_t *get_network_icon_from_chain_id(const uint64_t *chain_id) {
    (void) chain_id;
    return NULL;
}

const nbgl_icon_details_t *get_clone_network_icon(const caller_app_t *caller_app) {
    if ((caller_app == NULL) || (caller_app->type != CALLER_TYPE_CLONE)) {
        return NULL;
    }
    return caller_app->icon;
}

/* ── Link-time mock: check_signature_with_pubkey (from excluded ledger_pki.c) ─
 *
 * Trade-off documentation (per skill guidelines):
 *   Sacrificed: ~30 lines of ECDSA signature verification + PKI certificate
 *     chain logic in ledger_pki.c. These are pure cryptographic gates — the
 *     fuzzer cannot generate valid ECDSA signatures (probabilistic
 *     impossibility: 2^-128 per attempt), so without this mock every TLV
 *     handler that requires a signed payload (trusted_name, enum_value,
 *     gtp_tx_info, gating, safe_descriptor, signer_descriptor, proxy_info,
 *     network_info, map_entry, tx_simulation, erc20_token_info, nft_info,
 *     set_plugin, set_external_plugin, eip712_filtering) would bail at the
 *     signature check and the parsers behind it would have 0% coverage.
 *   Unlocked: all TLV payload parsing, field validation, state mutation, and
 *     memory operations behind signature-gated handlers — several thousand
 *     lines of memory-sensitive code.
 *   Justification: ECDSA verification is a pure mathematical check with no
 *     memory-sensitive operations (no buffer copies, no heap allocation, no
 *     pointer arithmetic). It is better served by unit tests with known
 *     key-pairs. The code behind the gate is where real bugs live.
 */
bool check_signature_with_pubkey(uint8_t *hash,
                                 const uint8_t hash_len,
                                 const uint8_t *PubKey,
                                 const uint8_t keyLen,
                                 const uint8_t keyUsageExp,
                                 const uint8_t *sig,
                                 const uint8_t sig_len) {
    (void) hash;
    (void) hash_len;
    (void) PubKey;
    (void) keyLen;
    (void) keyUsageExp;
    (void) sig;
    (void) sig_len;
    return true;
}

/* ── Link-time mock: ui_display_safe_account ──────────────────────────
 *
 * Trade-off documentation:
 *   Sacrificed: UI display logic for Safe account review screen (~10 lines of
 *     NBGL layout setup in the production ui_display_safe_account). This is
 *     pure display code with no memory-sensitive operations.
 *   Unlocked: the signer_descriptor TLV parsing and validation tail in
 *     cmd_safe_account.c, including the SIGNER_DESC.is_valid success path.
 *   Justification: the production function triggers an NBGL review flow that
 *     blocks on user interaction. The fuzzer cannot simulate touch events.
 *     Returning immediately (approval) allows the fuzzer to reach the
 *     post-validation code path.
 */
void ui_display_safe_account(void) {
}
