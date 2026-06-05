/**
 * @file mock.c
 * @brief Host-side stubs and __wrap_* targets for the unit-test suite.
 *
 * Everything in here is either:
 *   1. a stand-in for an SDK symbol we cannot link in host mode (lib_cxng,
 *      lib_alloc, syscall surface), or
 *   2. a controllable wrap for an app function whose real implementation
 *      sits on the device-only signing path.
 *
 * Most symbols are __attribute__((weak)) so a single test can still
 * substitute its own strong override (e.g. to count invocations, capture
 * arguments, or sequence behavior through cmocka mock()). Shared state
 * (return flags, counters, default tickers) lives next to its wrap below
 * and is re-exported through mocks/wraps.h so the tests just `#include
 * "wraps.h"` and toggle the global.
 *
 * Symbols are grouped by their origin -- SDK first, then the app's own
 * source tree, then the ethereum-plugin-sdk submodule. Each group is
 * delimited by a banner that names the upstream header / source.
 */

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "buffer.h"
#include "read.h"
#include "cx_errors.h"
#include "tlv_library.h"
#include "tlv_apdu.h"

// =============================================================================
// Forward-declared SDK opaque types
// =============================================================================
// Several SDK headers (lcx_sha256.h, lcx_sha3.h, lcx_hash.h, exceptions.h)
// are gated behind HAVE_SHA256 / HAVE_SHA3 / etc. macros that aren't
// defined for every test target, and exceptions.h pulls the syscall
// surface in turn. Forward-declare the types we only need by name so
// mock.c compiles standalone for every target.

typedef struct cx_sha256_s cx_sha256_t;
typedef struct cx_sha3_s cx_sha3_t;
typedef struct cx_hash_header_s cx_hash_t;
typedef struct try_context_s try_context_t;

// =============================================================================
// Shared __wrap_* state -- declared in mocks/wraps.h
// =============================================================================
// Tests `#include "wraps.h"` to get the extern decls and flip these
// from setup or in dedicated failure cases. A test that needs richer
// behavior overrides the matching __wrap_* below with a strong def.

bool g_sig_check_ret = true;               // __wrap_check_signature_with_pubkey
int g_sig_check_calls = 0;                 // bumped on every sig-check invocation
bool g_finalize_hash_ret = true;           // __wrap_finalize_hash
uint64_t g_tx_chain_id = 1;                // __wrap_get_tx_chain_id
const char *g_displayable_ticker = "ETH";  // __wrap_get_displayable_ticker
bool g_parsebip32_force_null = false;      // __wrap_parseBip32 short-circuit
uint32_t g_keccak_init_ret = CX_OK;        // __wrap_cx_keccak_init_no_throw

uint16_t g_get_public_key_ret = 0x9000;      // SWO_SUCCESS for get_public_key
bool g_getEthDisplayableAddress_ret = true;  // true for getEthDisplayableAddress
bool g_amountToString_ret = true;            // true for amountToString
bool g_get_network_as_string_ret = true;     // true for get_network_as_string

// Noreturn handshake: tests arm the jump, run the stmt inside the
// EXPECT_NORETURN() macro (wraps.h), and inspect g_noreturn_calls.
jmp_buf g_noreturn_jmp;
bool g_noreturn_armed = false;
int g_noreturn_calls = 0;

// s_tx_info is an unnamed-struct typedef in gtp_tx_info.h, so we can't
// forward-declare it here without pulling that header's chain. Use
// const void * and let tests assign s_tx_info* implicitly.
const void *g_tx_info_ret = NULL;  // __wrap_get_current_tx_info

// =============================================================================
// BOLOS_SDK -- include/os_pic.h
// =============================================================================
// PIC translation for SDK's tlv_library.c. The real impl rewrites a NVRAM
// address to RAM; in host tests addresses are already in RAM so identity
// is correct.

void *pic(void *addr) {
    return addr;
}

// =============================================================================
// BOLOS_SDK -- include/os_utils.h
// =============================================================================
// Pure helpers from os.c. The real source pulls BSS/syscall machinery so
// we mirror them here instead of linking os.c. All weak so a test can
// override locally if it ever wants a different behavior.

int bytes_to_lowercase_hex(char *out, size_t outl, const void *value, size_t len) {
    const uint8_t *bytes = (const uint8_t *) value;
    const char *hex = "0123456789abcdef";

    if (outl < 2 * len + 1) {
        *out = '\0';
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        *out++ = hex[(bytes[i] >> 4) & 0xf];
        *out++ = hex[bytes[i] & 0xf];
    }
    *out = '\0';
    return 0;
}

bool is_printable_string(const char *str, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (str[i] < 0x20 || str[i] > 0x7E) {
            return false;
        }
    }
    return true;
}

__attribute__((weak)) bool is_zeroes_buffer(const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *) buf;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != 0) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// BOLOS_SDK -- include/ledger_assert_internals.h
// =============================================================================

__attribute__((noreturn)) void assert_exit(bool confirm) {
    (void) confirm;
    // The SDK declares assert_exit as noreturn -- match that on the test side too.
    while (1) {
    }
}

// =============================================================================
// BOLOS_SDK -- include/exceptions.h + include/os_lib.h
// =============================================================================
// Try/catch primitives plus the os_lib_call syscall. Silent stubs by
// default; test_eth_plugin_handler overrides os_longjmp with fail_msg
// to catch unexpected exception paths.

__attribute__((weak)) try_context_t *try_context_set(try_context_t *ctx) {
    (void) ctx;
    return NULL;
}

__attribute__((weak)) try_context_t *try_context_get(void) {
    return NULL;
}

__attribute__((weak)) __attribute__((noreturn)) void os_longjmp(unsigned int exception) {
    (void) exception;
    while (1) {
    }
}

__attribute__((weak)) void os_lib_call(unsigned int *call_parameters) {
    (void) call_parameters;
}

// =============================================================================
// BOLOS_SDK -- lib_cxng/include/lcx_sha256.h
// =============================================================================
// Two flavors: the bare `cx_sha256_init_no_throw` resolves for targets
// that link production code referencing it directly, and the
// `__wrap_cx_hash_sha256` (+ unwrapped sibling) covers both linker
// modes. Outputs are deterministic so callers can assert a known
// pattern; test_eth2_plugin keeps a strong override that fills with
// in[0] instead of 0xAB.

__attribute__((weak)) cx_err_t cx_sha256_init_no_throw(cx_sha256_t *hash) {
    (void) hash;
    return CX_OK;
}

__attribute__((weak)) size_t __wrap_cx_hash_sha256(const uint8_t *in,
                                                   size_t len,
                                                   uint8_t *out,
                                                   size_t out_len) {
    (void) in;
    (void) len;
    if (out != NULL && out_len > 0) {
        memset(out, 0xAB, out_len);
    }
    return out_len;
}

// Non-wrapped variant for targets that don't pass --wrap=cx_hash_sha256
// (e.g. test_cmd_set_plugin). Same 0xAB-fill body.
__attribute__((weak)) size_t cx_hash_sha256(const uint8_t *in,
                                            size_t len,
                                            uint8_t *out,
                                            size_t out_len) {
    (void) in;
    (void) len;
    if (out != NULL && out_len > 0) {
        memset(out, 0xAB, out_len);
    }
    return out_len;
}

// cx_sha256_hash() is a static inline in lcx_sha256.h that forwards to
// cx_sha256_hash_iovec; the iovec entry-point is where the linker can
// catch the call. The default returns CX_OK and zero-fills the digest;
// tests that need to drive return values / canned outputs (e.g. test
// vectors against a known hash) install a strong cmocka-backed override
// locally. Forward-decl cx_iovec_s so we don't pull lcx_*.h here.
struct cx_iovec_s;
__attribute__((weak)) cx_err_t __wrap_cx_sha256_hash_iovec(const struct cx_iovec_s *iovec,
                                                           size_t iovec_len,
                                                           uint8_t *digest) {
    (void) iovec;
    (void) iovec_len;
    if (digest != NULL) {
        memset(digest, 0, 32);
    }
    return CX_OK;
}

// =============================================================================
// BOLOS_SDK -- lib_cxng/include/lcx_sha3.h
// =============================================================================
// cx_keccak_256_hash is `static inline` in the SDK header (it just forwards
// to cx_keccak_256_hash_iovec), so we never redefine it directly. The iovec
// entry-point is defined here without pulling lcx_sha3.h; production code
// calls it through the inline wrapper.

struct cx_iovec_s;
uint32_t cx_keccak_256_hash_iovec(const struct cx_iovec_s *iovec,
                                  size_t iovec_len,
                                  uint8_t *digest) {
    (void) iovec;
    (void) iovec_len;
    (void) digest;
    return CX_OK;
}

uint32_t cx_keccak_256_hash(const uint8_t *in, size_t in_len, uint8_t *out) {
    (void) in;
    (void) in_len;
    if (out) {
        for (size_t i = 0; i < 32; i++) {
            out[i] = (uint8_t) (i * 7);
        }
    }
    return CX_OK;
}

__attribute__((weak)) uint32_t __wrap_cx_keccak_init_no_throw(cx_sha3_t *hash, size_t size) {
    (void) hash;
    (void) size;
    return g_keccak_init_ret;
}

// Non-wrapped variant for targets that don't pass --wrap=cx_sha3_init_no_throw
// (e.g. test_tx_ctx). Same trivial body, untracked state.
__attribute__((weak)) uint32_t cx_sha3_init_no_throw(cx_sha3_t *hash, size_t size) {
    (void) hash;
    (void) size;
    return CX_OK;
}

// =============================================================================
// BOLOS_SDK -- lib_cxng/include/lcx_math.h
// =============================================================================
// cx_math_mult_no_throw has two variants: the bare stub returns CX_OK
// without touching `r` (fine for targets that don't care about the
// product), and __wrap_cx_math_mult_no_throw does a real big-endian
// schoolbook multiply for targets (test_uint128, test_logic_sign_tx_fee)
// that need verifiable results. test_uint256 overrides the wrap with a
// cmocka-driven version so it can assert on the inputs.

uint32_t cx_math_mult_no_throw(uint8_t *r, const uint8_t *a, const uint8_t *b, size_t len) {
    (void) r;
    (void) a;
    (void) b;
    (void) len;
    return CX_OK;
}

__attribute__((weak)) uint32_t __wrap_cx_math_mult_no_throw(uint8_t *r,
                                                            const uint8_t *a,
                                                            const uint8_t *b,
                                                            size_t len) {
    memset(r, 0, 2 * len);
    for (size_t i = 0; i < len; i++) {
        size_t a_idx = len - 1 - i;
        uint32_t carry = 0;
        for (size_t j = 0; j < len; j++) {
            size_t b_idx = len - 1 - j;
            size_t r_idx = 2 * len - 1 - (i + j);
            uint32_t prod = (uint32_t) a[a_idx] * (uint32_t) b[b_idx] + (uint32_t) r[r_idx] + carry;
            r[r_idx] = (uint8_t) (prod & 0xFF);
            carry = prod >> 8;
        }
        if (carry != 0) {
            size_t r_idx = 2 * len - 1 - (i + len);
            uint32_t v = (uint32_t) r[r_idx] + carry;
            r[r_idx] = (uint8_t) (v & 0xFF);
        }
    }
    return CX_OK;
}

// =============================================================================
// BOLOS_SDK -- lib_alloc/app_mem_utils.h
// =============================================================================
// libc-backed implementation of the lib_alloc API. The real impl is a
// fixed-size pool allocator; in host tests we just delegate to malloc.

static void *test_heap = NULL;
static size_t test_heap_size = 0;

bool mem_utils_init(void *heap_start, size_t heap_size) {
    test_heap = heap_start;
    test_heap_size = heap_size;
    return true;
}

void *mem_utils_alloc(size_t size, bool permanent, const char *file, int line) {
    (void) permanent;
    (void) file;
    (void) line;
    return malloc(size);
}

void *mem_utils_realloc(void *ptr, size_t size, const char *file, int line) {
    (void) file;
    (void) line;
    return realloc(ptr, size);
}

void mem_utils_free(void *ptr, const char *file, int line) {
    (void) file;
    (void) line;
    free(ptr);
}

void mem_utils_free_and_null(void **buffer, const char *file, int line) {
    (void) file;
    (void) line;
    if (*buffer != NULL) {
        free(*buffer);
        *buffer = NULL;
    }
}

char *mem_utils_strdup(const char *s, const char *file, int line) {
    (void) file;
    (void) line;
    return strdup(s);
}

bool mem_utils_calloc(void **buffer, uint16_t size, bool permanent, const char *file, int line) {
    (void) permanent;
    (void) file;
    (void) line;
    // The real lib_alloc impl does NOT pre-free *buffer; it just writes a
    // fresh pointer. Many call sites (e.g. gtp_field_table.c) pass an
    // uninitialized stack pointer, so a pre-free here would invoke free()
    // on junk.
    if (size == 0) {
        return true;
    }
    if ((*buffer = malloc(size)) == NULL) {
        return false;
    }
    memset(*buffer, 0, size);
    return true;
}

// =============================================================================
// BOLOS_SDK -- lib_tlv (and apps/src/tlv_utils.c)
// =============================================================================

bool tlv_parse(const uint8_t *payload, uint16_t size, void *handler, void *context) {
    (void) payload;
    (void) size;
    (void) handler;
    (void) context;
    return true;
}

// check_challenge lives in src/tlv_utils.c and gates trusted-name /
// proxy / safe-account descriptor freshness. Tests never replay so we
// always accept.
bool check_challenge(uint32_t received_challenge) {
    (void) received_challenge;
    return true;
}

// =============================================================================
// BOLOS_SDK -- lib_standard_app/main.c (app entry point)
// =============================================================================

// app_exit and send_swap_error_simple are noreturn in production
// (SDK and ledger lib_standard_app respectively). When tests exercise
// a code path that *should* call one of them, they arm g_noreturn_armed
// (via EXPECT_NORETURN in wraps.h) and we longjmp back so the
// assertion can inspect g_noreturn_calls. When not armed we fall
// through to while(1) -- a runaway call MUST stall the test instead
// of returning normally, since callers rely on the noreturn contract.
//
// app_quit is *not* noreturn in production (shared_context.h declares
// it `void app_quit(void)`) -- it's called right before `while(1)` at
// every callsite as a defence in depth. We keep that contract here:
// app_quit just increments the counter and returns. Tests assert
// app_quit was triggered by reading g_noreturn_calls after the call
// under test returns.

__attribute__((weak)) __attribute__((noreturn)) void app_exit(void) {
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

__attribute__((weak)) void app_quit(void) {
    g_noreturn_calls++;
}

__attribute__((weak)) __attribute__((noreturn)) void send_swap_error_simple(
    uint16_t status_word,
    uint8_t common_error_code,
    uint8_t application_specific_error_code) {
    (void) status_word;
    (void) common_error_code;
    (void) application_specific_error_code;
    g_noreturn_calls++;
    if (g_noreturn_armed) longjmp(g_noreturn_jmp, 1);
    while (1) {
    }
}

// =============================================================================
// app/src/ledger_pki.c (via public_keys.h)
// =============================================================================
// Wrapped through --wrap=check_signature_with_pubkey. g_sig_check_ret
// drives the outcome, g_sig_check_calls counts invocations.

__attribute__((weak)) bool __wrap_check_signature_with_pubkey(uint8_t *buffer,
                                                              const uint8_t bufLen,
                                                              const uint8_t *PubKey,
                                                              const uint8_t keyLen,
                                                              const uint8_t keyUsageExp,
                                                              const uint8_t *signature,
                                                              const uint8_t sigLen) {
    (void) buffer;
    (void) bufLen;
    (void) PubKey;
    (void) keyLen;
    (void) keyUsageExp;
    (void) signature;
    (void) sigLen;
    g_sig_check_calls++;
    return g_sig_check_ret;
}

// =============================================================================
// app/src/hash_bytes.c
// =============================================================================
// __wrap_finalize_hash zeroes the output and returns g_finalize_hash_ret.
// __wrap_hash_nbytes is a pure no-op (the actual bytes don't matter for
// any test). Outliers (test_cmd_sign_message uses 0xAB fill, test_tx_ctx
// memcpys a captured buffer) keep strong overrides.

__attribute__((weak)) bool __wrap_finalize_hash(cx_hash_t *hash_ctx, uint8_t *out, size_t out_len) {
    (void) hash_ctx;
    memset(out, 0, out_len);
    return g_finalize_hash_ret;
}

__attribute__((weak)) void __wrap_hash_nbytes(const uint8_t *bytes, size_t n, cx_hash_t *hash_ctx) {
    (void) bytes;
    (void) n;
    (void) hash_ctx;
}

// =============================================================================
// app/src/tlv_apdu.c
// =============================================================================
// Centralised __wrap_tlv_from_apdu for the descriptor-parser dispatchers
// (proxy_info, trusted_name, enum_value, network_info, safe_account).
// Captures (first_chunk, lc, handler), optionally invokes the handler
// with an empty buffer (gated by g_tlv_from_apdu_invoke_handler), and
// returns g_tlv_from_apdu_ret. Tests reset captures and set the return
// in their fixture; a test that needs richer behavior (counters across
// re-entry, handler-specific capture, multi-call sequencing) keeps a
// strong local override.

int g_tlv_from_apdu_calls = 0;
bool g_tlv_from_apdu_first_chunk = false;
uint8_t g_tlv_from_apdu_lc = 0;
void *g_tlv_from_apdu_handler = NULL;
bool g_tlv_from_apdu_invoke_handler = false;
int g_tlv_from_apdu_ret = TLV_APDU_SUCCESS;

__attribute__((weak)) e_tlv_apdu_ret __wrap_tlv_from_apdu(bool first_chunk,
                                                          uint8_t lc,
                                                          const uint8_t *payload,
                                                          f_tlv_payload_handler handler) {
    (void) payload;
    g_tlv_from_apdu_calls++;
    g_tlv_from_apdu_first_chunk = first_chunk;
    g_tlv_from_apdu_lc = lc;
    // Function-pointer to void* is UB per ISO C; union sidesteps -Wpedantic.
    union {
        f_tlv_payload_handler fn;
        void *ptr;
    } u = {.fn = handler};
    g_tlv_from_apdu_handler = u.ptr;
    if (g_tlv_from_apdu_invoke_handler && handler != NULL) {
        buffer_t buf = {.ptr = NULL, .size = 0, .offset = 0};
        // The callback's return doesn't affect tlv_from_apdu's own
        // return -- production code surfaces the same outcome both up
        // the tlv_from_apdu stack and onto the SWO. Tests that want to
        // observe the inner callback outcome assert through the wrapped
        // leaves.
        (void) handler(&buf);
    }
    return (e_tlv_apdu_ret) g_tlv_from_apdu_ret;
}

// =============================================================================
// app/src/network.c
// =============================================================================

__attribute__((weak)) uint64_t __wrap_get_tx_chain_id(void) {
    return g_tx_chain_id;
}

// chain_config_t is type-erased to const void * here so we don't drag in
// shared_context.h. Production callers see the real signature through
// network.h.
__attribute__((weak)) const char *__wrap_get_displayable_ticker(const uint64_t *chain_id,
                                                                const void *config) {
    (void) chain_id;
    (void) config;
    return g_displayable_ticker;
}

// Non-wrapped sibling for targets that don't pass
// --wrap=get_displayable_ticker (e.g. test_tx_ctx).
__attribute__((weak)) const char *get_displayable_ticker(const uint64_t *chain_id,
                                                         const void *config) {
    (void) chain_id;
    (void) config;
    return g_displayable_ticker;
}

// Non-wrapped get_tx_chain_id for targets without
// --wrap=get_tx_chain_id (test_eth_ustream_*, test_cmd_sign_tx,
// test_tx_ctx). All share the same g_tx_chain_id global; tests that
// want a different default set it in their reset() before driving
// the code under test (ustream tests reset to 0 to mirror the
// "chain unknown until parser observes v/chainId" state).
__attribute__((weak)) uint64_t get_tx_chain_id(void) {
    return g_tx_chain_id;
}

// Fallback ticker for unknown chains. The real def lives in
// src/network.c (linked by test_network) and overrides this weak.
__attribute__((weak)) const char g_unknown_ticker[] = "???";

// =============================================================================
// app/src/features/generic_tx_parser/gtp_field_table.c
// =============================================================================
// e_param_type is opaque-passed as int here; the wrapped __wrap_ variant
// (used by 11 cmocka-driven test_param_* files) is too divergent to
// share, but tests that exercise tx_ctx end-to-end (test_tx_ctx) just
// need the symbol to resolve.

__attribute__((weak)) bool add_to_field_table(int type,
                                              const char *key,
                                              const char *value,
                                              const void *extra) {
    (void) type;
    (void) key;
    (void) value;
    (void) extra;
    return true;
}

__attribute__((weak)) bool set_intent_field(const char *value) {
    (void) value;
    return true;
}

// =============================================================================
// app/src/features/provide_trusted_name/trusted_name.c
// =============================================================================
// Return type and array params are type-erased to void * to avoid
// dragging trusted_name.h's common_utils.h chain through every target.
// Production code sees the real prototype through trusted_name.h.

__attribute__((weak)) const void *get_trusted_name(uint8_t type_count,
                                                   const void *types,
                                                   uint8_t source_count,
                                                   const void *sources,
                                                   const uint64_t *chain_id,
                                                   const uint8_t *addr) {
    (void) type_count;
    (void) types;
    (void) source_count;
    (void) sources;
    (void) chain_id;
    (void) addr;
    return NULL;
}

// =============================================================================
// ethereum-plugin-sdk/src/common_utils.c
// =============================================================================
// Display helpers used by the GCS field renderer and the swap-mode
// integration. test_tx_ctx (process_empty_tx) is the only consumer
// today; bodies are trivial pass/no-ops.

// amountToString / getEthDisplayableAddress write a deterministic
// placeholder into `out` when the *_ret global is true so tests that
// inspect strings.common.* can assert against a known value. Tests that
// want to drive a failure path flip the global to false; tests that need
// a specific output content keep a strong local override.
//
// Both helpers have a strong definition in
// ethereum-plugin-sdk/src/common_utils.c. Tests that link common_utils.c
// (most of them, via PLUGIN_DIR) AND want to short-circuit the real impl
// add --wrap=amountToString / --wrap=getEthDisplayableAddress to their
// cmake target. Tests that don't link common_utils.c get the bare
// WEAK below directly.

static bool _stub_amount_to_string(char *out_buffer, size_t out_buffer_size) {
    if (g_amountToString_ret && out_buffer != NULL && out_buffer_size > 0) {
        strncpy(out_buffer, "1.5", out_buffer_size);
        out_buffer[out_buffer_size - 1] = '\0';
    }
    return g_amountToString_ret;
}

__attribute__((weak)) bool amountToString(const uint8_t *amount,
                                          uint8_t amount_len,
                                          uint8_t decimals,
                                          const char *ticker,
                                          char *out_buffer,
                                          size_t out_buffer_size) {
    (void) amount;
    (void) amount_len;
    (void) decimals;
    (void) ticker;
    return _stub_amount_to_string(out_buffer, out_buffer_size);
}

__attribute__((weak)) bool __wrap_amountToString(const uint8_t *amount,
                                                 uint8_t amount_len,
                                                 uint8_t decimals,
                                                 const char *ticker,
                                                 char *out_buffer,
                                                 size_t out_buffer_size) {
    (void) amount;
    (void) amount_len;
    (void) decimals;
    (void) ticker;
    return _stub_amount_to_string(out_buffer, out_buffer_size);
}

static bool _stub_eth_displayable_address(char *out, size_t out_size) {
    if (g_getEthDisplayableAddress_ret && out != NULL && out_size > 0) {
        strncpy(out, "0xdeadbeef", out_size);
        out[out_size - 1] = '\0';
    }
    return g_getEthDisplayableAddress_ret;
}

__attribute__((weak)) bool getEthDisplayableAddress(const uint8_t *in,
                                                    char *out,
                                                    size_t out_size,
                                                    uint64_t chain_id) {
    (void) in;
    (void) chain_id;
    return _stub_eth_displayable_address(out, out_size);
}

__attribute__((weak)) bool __wrap_getEthDisplayableAddress(const uint8_t *in,
                                                           char *out,
                                                           size_t out_size,
                                                           uint64_t chain_id) {
    (void) in;
    (void) chain_id;
    return _stub_eth_displayable_address(out, out_size);
}

// get_public_key copies a deterministic 20-byte placeholder address
// when the *_ret is SWO_SUCCESS so the caller's downstream formatting
// has a stable input.

__attribute__((weak)) uint16_t get_public_key(uint8_t *out, uint8_t out_size) {
    if (g_get_public_key_ret == 0x9000 && out != NULL && out_size >= 20) {
        memset(out, 0xAB, 20);
    }
    return g_get_public_key_ret;
}

// get_network_as_string writes "Ethereum" into out when the ret
// global is true; otherwise leaves the buffer alone and returns false.

__attribute__((weak)) bool get_network_as_string(char *out, size_t out_len) {
    if (g_get_network_as_string_ret && out != NULL && out_len > 0) {
        strncpy(out, "Ethereum", out_len);
        out[out_len - 1] = '\0';
    }
    return g_get_network_as_string_ret;
}

// =============================================================================
// app/src/features/get_challenge/cmd_get_challenge.c
// =============================================================================
// Most tests don't care about the post-read challenge regen; stateful
// overrides (test_safe_descriptors, test_proxy_info) count invocations.

__attribute__((weak)) void roll_challenge(void) {
}

// =============================================================================
// app/src/features/generic_tx_parser/gtp_data_path.c
// =============================================================================
// All three weak so test_data_path can link the real gtp_data_path.c
// without a multiple-definition error.

__attribute__((weak)) bool handle_data_path_struct(const void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

__attribute__((weak)) void data_path_cleanup(const void *collection) {
    (void) collection;
}

__attribute__((weak)) bool data_path_get(const void *data_path, void *collection) {
    (void) data_path;
    (void) collection;
    return true;
}

// =============================================================================
// app/src/features/generic_tx_parser/tx_ctx.c
// =============================================================================
// The unwrapped getters are needed by code that links production tx_ctx
// callers without --wrap; the wrapped __wrap_get_current_tx_info is for
// gtp param tests that need to flip the result. test_tx_ctx itself links
// the real tx_ctx.c so all four are weak.

__attribute__((weak)) const uint8_t *get_current_tx_to(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *get_current_tx_from(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *get_current_tx_info(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *get_current_tx_amount(void) {
    return NULL;
}

__attribute__((weak)) const void *__wrap_get_current_tx_info(void) {
    return g_tx_info_ret;
}

// =============================================================================
// app/src/features/generic_tx_parser/gtp_value.c
// =============================================================================
// value_cleanup pairs with value_get; tests almost never observe it.
// Forward-declare its operand types to skip the heavy header chain.

struct s_value;
struct s_parsed_value_collection;

__attribute__((weak)) void __wrap_value_cleanup(
    const struct s_value *value,
    const struct s_parsed_value_collection *collection) {
    (void) value;
    (void) collection;
}

// =============================================================================
// app/src/features/provide_map_entry/map_entry.c
// =============================================================================

__attribute__((weak)) const void *get_matching_map_entry(uint8_t id,
                                                         const uint8_t *key,
                                                         uint8_t key_size) {
    (void) id;
    (void) key;
    (void) key_size;
    return NULL;
}

// =============================================================================
// app/src/features/sign_tx/logic_sign_tx.c link-fillers
// =============================================================================
// logic_sign_tx.c references a handful of plugin / UI / calldata helpers
// that the finalize-side tests don't exercise (most live inside
// custom_processor). The defaults below let any test that links
// logic_sign_tx.c resolve them without redeclaring the boilerplate.
// Tests that DO exercise the plugin path keep a strong local override.

struct txContext_t;
__attribute__((weak)) bool copy_tx_data(struct txContext_t *context,
                                        uint8_t *out,
                                        uint32_t length) {
    (void) context;
    (void) out;
    (void) length;
    return true;
}

__attribute__((weak)) void eth_plugin_prepare_init(void *msg,
                                                   const uint8_t *pluginName,
                                                   uint8_t pluginNameLength) {
    (void) msg;
    (void) pluginName;
    (void) pluginNameLength;
}

__attribute__((weak)) bool eth_plugin_perform_init(uint8_t *contractAddress, void *msg) {
    (void) contractAddress;
    (void) msg;
    return true;
}

__attribute__((weak)) void eth_plugin_prepare_finalize(void *msg) {
    (void) msg;
}

__attribute__((weak)) void eth_plugin_prepare_provide_info(void *msg) {
    (void) msg;
}

__attribute__((weak)) void eth_plugin_prepare_provide_parameter(void *msg,
                                                                const uint8_t *param,
                                                                uint32_t paramOffset) {
    (void) msg;
    (void) param;
    (void) paramOffset;
}

__attribute__((weak)) void *get_matching_asset_info(const uint64_t *chain_id,
                                                    const uint8_t *address) {
    (void) chain_id;
    (void) address;
    return NULL;
}

__attribute__((weak)) void ui_confirm_parameter(void) {
}

__attribute__((weak)) void ui_confirm_selector(void) {
}

// get_root_calldata + calldata_get_selector live in tx_ctx.c in production.
// Test targets that link tx_ctx.c get the real impl; targets that don't
// (e.g. logic_sign_tx tests) fall through to these WEAKs.

struct s_calldata;
__attribute__((weak)) struct s_calldata *get_root_calldata(void) {
    return NULL;
}

__attribute__((weak)) const uint8_t *calldata_get_selector(const struct s_calldata *node) {
    (void) node;
    return NULL;
}

// =============================================================================
// app/src/main.c -- BIP-32 path parsing
// =============================================================================
// parseBip32 reads a length byte then N*4 path bytes. The stub mirrors
// the byte arithmetic so callers that consume the returned pointer see
// the right offset; g_parsebip32_force_null flips it to NULL for the
// negative tests.

__attribute__((weak)) const uint8_t *__wrap_parseBip32(const uint8_t *dataBuffer,
                                                       uint8_t *dataLength,
                                                       void *bip32) {
    (void) bip32;
    if (g_parsebip32_force_null) return NULL;
    if (*dataLength < 1) return NULL;
    uint8_t count = *dataBuffer;
    if ((size_t) *dataLength < 1 + (size_t) count * 4) return NULL;
    dataBuffer += 1 + count * 4;
    *dataLength -= 1 + count * 4;
    return dataBuffer;
}

// =============================================================================
// ethereum-plugin-sdk/src/plugin_utils.c
// =============================================================================
// copy_parameter / copy_address are plugin-utils helpers. The real
// impls live in the submodule, but plugin_utils.c can't be linked
// because its MIN macro pulls os_math.h's syscall surface.
// PARAMETER_LENGTH (== 32) is hard-coded so we don't pull
// plugin_utils.h -> eth_plugin_interface.h -> os.h.

__attribute__((weak)) void copy_parameter(uint8_t *dst,
                                          const uint8_t *parameter,
                                          uint8_t dst_size) {
    size_t n = dst_size < 32u ? dst_size : 32u;
    memmove(dst, parameter, n);
}

__attribute__((weak)) void copy_address(uint8_t *dst, const uint8_t *parameter, uint8_t dst_size) {
    size_t n = dst_size < 20u ? dst_size : 20u;
    memmove(dst, parameter + (32u - n), n);
}
