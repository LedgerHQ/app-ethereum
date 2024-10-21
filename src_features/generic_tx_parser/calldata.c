#ifdef HAVE_GENERIC_TX_PARSER

#include <string.h>
#include "calldata.h"
#include "os_print.h"
#include "mem.h"

typedef struct {
    uint8_t *ptr;
    size_t size;
    size_t expected_size;
} s_calldata;

static s_calldata g_calldata = {0};

bool calldata_init(size_t size) {
    if ((g_calldata.ptr = mem_alloc(size)) == NULL) {
        return false;
    }
    g_calldata.expected_size = size;
    return true;
}

bool calldata_append(const uint8_t *buffer, size_t size) {
    if ((g_calldata.size + size) > g_calldata.expected_size) {
        return false;
    }
    memcpy(&g_calldata.ptr[g_calldata.size], buffer, size);
    g_calldata.size += size;
    return true;
}

void calldata_cleanup(void) {
    mem_dealloc(g_calldata.size);
    explicit_bzero(&g_calldata, sizeof(g_calldata));
}

static bool has_valid_calldata(const s_calldata *calldata) {
    if (calldata->ptr == NULL) {
        PRINTF("Error: no calldata!\n");
        return false;
    }
    if (g_calldata.size != g_calldata.expected_size) {
        PRINTF("Error: incomplete calldata!\n");
        return false;
    }
    return true;
}

const uint8_t *calldata_get_selector(void) {
    if (!has_valid_calldata(&g_calldata) || (g_calldata.size < CALLDATA_SELECTOR_SIZE)) {
        return NULL;
    }
    return g_calldata.ptr;
}

const uint8_t *calldata_get_chunk(int idx) {
    if (!has_valid_calldata(&g_calldata) ||
        (g_calldata.size < (CALLDATA_SELECTOR_SIZE + ((size_t) idx + 1) * CALLDATA_CHUNK_SIZE))) {
        return NULL;
    }
    return &g_calldata.ptr[CALLDATA_SELECTOR_SIZE + (idx * CALLDATA_CHUNK_SIZE)];
}

#endif  // HAVE_GENERIC_TX_PARSER
