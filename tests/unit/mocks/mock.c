#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "tlv_library.h"
#include "buffer.h"
#include "read.h"

// PIC function for SDK's tlv_library.c (identity function in test environment)
void *pic(void *addr) {
    return addr;
}

void assert_exit(bool confirm) {
    (void) confirm;
}

uint32_t cx_keccak_256_hash_iovec(void *iovec, size_t iovec_len, uint8_t *digest) {
    (void) iovec;
    (void) iovec_len;
    (void) digest;
    return 0;  // CX_OK
}

uint32_t cx_keccak_256_hash(const uint8_t *in, size_t in_len, uint8_t *out) {
    (void) in;
    (void) in_len;
    // Fill with deterministic test data for address checksumming
    if (out) {
        for (size_t i = 0; i < 32; i++) {
            out[i] = (uint8_t) (i * 7);  // Simple deterministic pattern
        }
    }
    return 0;  // CX_OK
}

uint32_t cx_math_mult_no_throw(uint8_t *r, const uint8_t *a, const uint8_t *b, size_t len) {
    (void) r;
    (void) a;
    (void) b;
    (void) len;
    return 0;  // CX_OK
}

bool handle_data_path_struct(const void *data, void *context) {
    (void) data;
    (void) context;
    return true;
}

bool tlv_parse(const uint8_t *payload, uint16_t size, void *handler, void *context) {
    (void) payload;
    (void) size;
    (void) handler;
    (void) context;
    return true;
}

void data_path_cleanup(const void *collection) {
    (void) collection;
}

bool data_path_get(const void *data_path, void *collection) {
    (void) data_path;
    (void) collection;
    return true;
}

const uint8_t *get_current_tx_to(void) {
    return NULL;
}

const uint8_t *get_current_tx_from(void) {
    return NULL;
}

const uint8_t *get_current_tx_info(void) {
    return NULL;
}

bool check_challenge(uint32_t received_challenge) {
    (void) received_challenge;
    return true;  // Always accept challenge in tests
}

// Memory management mocks - SDK lib_alloc compatible
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
    if (*buffer != NULL) {
        free(*buffer);
    }
    if (size == 0) {
        *buffer = NULL;
        return true;
    }
    if ((*buffer = malloc(size)) == NULL) {
        return false;
    }
    memset(*buffer, 0, size);
    return true;
}

const uint8_t *get_current_tx_amount(void) {
    return NULL;
}
