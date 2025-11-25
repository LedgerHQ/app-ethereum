#include <string.h>
#include <stdlib.h>

#include "cx_errors.h"
#include "cx_sha256.h"
#include "cx_sha3.h"
#include "buffer.h"
#include "lcx_ecfp.h"
#include "mem_alloc.h"

#include "bip32_utils.h"

/** MemorySanitizer does not wrap explicit_bzero https://github.com/google/sanitizers/issues/1507
 * which results in false positives when running MemorySanitizer.
 */
void memset_s(void *buffer, char c, size_t n) {
    if (buffer == NULL) return;

    volatile char *ptr = buffer;
    while (n--) *ptr++ = c;
}

void app_quit(void) {
}
void app_main(void) {
}

uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    UNUSED(sw);
    UNUSED(tx);
    UNUSED(reset);
    UNUSED(idle);
    return 0;
}

const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    UNUSED(dataBuffer);
    UNUSED(dataLength);
    UNUSED(bip32);
    return NULL;
}

mem_ctx_t mem_init(void *heap_start, size_t heap_size) {
    (void) heap_size;
    return heap_start;
}

void *mem_alloc(mem_ctx_t ctx, size_t nb_bytes) {
    (void) ctx;
    return malloc(nb_bytes);
}

void mem_free(mem_ctx_t ctx, void *ptr) {
    (void) ctx;
    free(ptr);
}

// APPs expect a specific length
cx_err_t cx_ecdomain_parameters_length(cx_curve_t cv, size_t *length) {
    (void) cv;
    *length = (size_t) 32;
    return 0x00000000;
}
