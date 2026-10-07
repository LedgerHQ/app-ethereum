/**
 * @file sdk_stubs.c
 * @brief Host-side stubs for SDK / platform symbols that cannot be mocked by
 *        CMock (syscalls, crypto primitives, allocator, and compiler built-ins).
 *
 * These stubs satisfy the linker for symbols that either (a) have no matching
 * header in the test include path, (b) are declared as `static inline` in the
 * SDK header, or (c) would require pulling in the full device SDK to compile.
 *
 * None of these symbols carry test-configurable behaviour; tests that need to
 * observe or drive specific outcomes for higher-level app functions should use
 * the CMock-generated mocks declared via MOCK_HEADERS in their cmake target.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cx_errors.h"

// =============================================================================
// Forward-declared SDK opaque types
// =============================================================================
// Several SDK headers are gated behind HAVE_SHA256 / HAVE_SHA3 / etc. macros
// that are not defined for every test target. Forward-declare the types we only
// need by name so sdk_stubs.c compiles standalone for every target.

typedef struct cx_sha256_s cx_sha256_t;
typedef struct cx_sha3_s cx_sha3_t;
typedef struct cx_hash_header_s cx_hash_t;
typedef struct try_context_s try_context_t;

// =============================================================================
// g_keccak_init_ret — only configurable crypto return value here
// =============================================================================
// Tests that need to drive cx_keccak_init_no_throw failure set this in setUp.
uint32_t g_keccak_init_ret = CX_OK;

// =============================================================================
// BOLOS_SDK -- include/os_utils.h
// =============================================================================
// Pure helpers from os.c. The real source pulls BSS/syscall machinery so we
// mirror them here instead of linking os.c.

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
    if (len == 0) {
        return true;
    }
    if (str == NULL) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (str[i] < 0x20 || str[i] > 0x7E) {
            return false;
        }
    }
    return true;
}

bool is_displayable_string(const char *str, size_t len) {
    if (!is_printable_string(str, len)) {
        return false;
    }
    // Everything left is in 0x20..0x7E, so a visible character is a non-space one.
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ' ') {
            return true;
        }
    }
    return false;
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
    while (1) {
    }
}

// =============================================================================
// BOLOS_SDK -- include/exceptions.h + include/os_lib.h
// =============================================================================

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

struct cx_iovec_s;
__attribute__((weak)) cx_err_t cx_sha256_hash_iovec(const struct cx_iovec_s *iovec,
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

__attribute__((weak)) uint32_t cx_keccak_256_hash_iovec(const struct cx_iovec_s *iovec,
                                                        size_t iovec_len,
                                                        uint8_t *digest) {
    (void) iovec;
    (void) iovec_len;
    (void) digest;
    return CX_OK;
}

__attribute__((weak)) uint32_t cx_keccak_256_hash(const uint8_t *in, size_t in_len, uint8_t *out) {
    (void) in;
    (void) in_len;
    if (out) {
        for (size_t i = 0; i < 32; i++) {
            out[i] = (uint8_t) (i * 7);
        }
    }
    return CX_OK;
}

__attribute__((weak)) uint32_t cx_keccak_init_no_throw(cx_sha3_t *hash, size_t size) {
    (void) hash;
    (void) size;
    return g_keccak_init_ret;
}

__attribute__((weak)) uint32_t cx_sha3_init_no_throw(cx_sha3_t *hash, size_t size) {
    (void) hash;
    (void) size;
    return CX_OK;
}

// =============================================================================
// BOLOS_SDK -- lib_cxng/include/lcx_math.h
// =============================================================================

__attribute__((weak)) uint32_t cx_math_mult_no_throw(uint8_t *r,
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
// libc-backed implementation of the lib_alloc API.

static void *test_heap = NULL;
static size_t test_heap_size = 0;

bool mem_utils_init(void *heap_start, size_t heap_size) {
    test_heap = heap_start;
    test_heap_size = heap_size;
    return true;
}

__attribute__((weak)) void *mem_utils_alloc(size_t size,
                                            bool permanent,
                                            const char *file,
                                            int line) {
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

__attribute__((weak)) void mem_utils_free_and_null(void **buffer, const char *file, int line) {
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

__attribute__((weak)) bool mem_utils_calloc(void **buffer,
                                            uint16_t size,
                                            bool permanent,
                                            const char *file,
                                            int line) {
    (void) permanent;
    (void) file;
    (void) line;
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
// ethereum-plugin-sdk/src/plugin_utils.c
// =============================================================================
// PARAMETER_LENGTH (== 32) is hard-coded to avoid pulling plugin_utils.h.

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

// =============================================================================
// Unity compatibility stubs
// =============================================================================
// Unity references setUp()/tearDown() at link time. Provide weak no-op stubs
// so the linker is satisfied; any test file that needs real per-test setup
// can override them with a strong definition.

__attribute__((weak)) void setUp(void) {
}
__attribute__((weak)) void tearDown(void) {
}
