#include <string.h>
#include "os_math.h"  // MIN
#include "calldata.h"
#include "os_print.h"
#include "mem.h"
#include "mem_utils.h"
#include "list.h"

s_calldata *calldata_init(size_t size, const uint8_t selector[CALLDATA_SELECTOR_SIZE]) {
    s_calldata *calldata;

    if ((calldata = app_mem_alloc(sizeof(*calldata))) == NULL) {
        return NULL;
    }
    explicit_bzero(calldata, sizeof(*calldata));
    calldata->expected_size = size;
    calldata_set_selector(calldata, selector);
    return calldata;
}

bool calldata_set_selector(s_calldata *calldata, const uint8_t selector[CALLDATA_SELECTOR_SIZE]) {
    if ((calldata == NULL) || (selector == NULL)) {
        return false;
    }
    memcpy(calldata->selector, selector, sizeof(calldata->selector));
    return true;
}

static bool compress_chunk(s_calldata *calldata) {
    uint8_t strip_left = 0;
    uint8_t strip_right = 0;
    uint8_t start_idx;
    s_calldata_chunk *chunk;

    if (calldata == NULL) {
        return false;
    }

    if ((chunk = app_mem_alloc(sizeof(*chunk))) == NULL) {
        return false;
    }
    explicit_bzero(chunk, sizeof(*chunk));
    for (int i = 0; (i < CALLDATA_CHUNK_SIZE) && (calldata->chunk[i] == 0x00); ++i) {
        strip_left += 1;
    }
    for (int i = CALLDATA_CHUNK_SIZE - 1; (i >= 0) && (calldata->chunk[i] == 0x00); --i) {
        strip_right += 1;
    }
    if (strip_left >= strip_right) {
        chunk->dir = CHUNK_STRIP_LEFT;
        chunk->size = CALLDATA_CHUNK_SIZE - strip_left;
        start_idx = strip_left;
    } else {
        chunk->dir = CHUNK_STRIP_RIGHT;
        chunk->size = CALLDATA_CHUNK_SIZE - strip_right;
        start_idx = 0;
    }
    if (chunk->size > 0) {
        if ((chunk->buf = app_mem_alloc(chunk->size)) == NULL) {
            app_mem_free(chunk);
            return false;
        }
        memcpy(chunk->buf, calldata->chunk + start_idx, chunk->size);
    }
    flist_push_back((s_flist_node **) &calldata->chunks, (s_flist_node *) chunk);
    return true;
}

static bool decompress_chunk(const s_calldata_chunk *chunk, uint8_t *out) {
    size_t diff;

    if ((chunk == NULL) || (out == NULL)) {
        // Should never happen, but just in case
        return false;
    }
    if ((chunk->buf == NULL) || (chunk->size == 0)) {
        // nothing to decompress
        explicit_bzero(out, CALLDATA_CHUNK_SIZE);
        return true;
    }
    diff = CALLDATA_CHUNK_SIZE - chunk->size;
    if (chunk->dir == CHUNK_STRIP_LEFT) {
        explicit_bzero(out, diff);
        memcpy(&out[diff], chunk->buf, chunk->size);
    } else {
        memcpy(out, chunk->buf, chunk->size);
        explicit_bzero(&out[chunk->size], diff);
    }
    return true;
}

bool calldata_append(s_calldata *calldata, const uint8_t *buffer, size_t size) {
    uint8_t cpy_length;

    if (calldata == NULL) return false;
    if ((calldata->received_size + size) > calldata->expected_size) {
        return false;
    }

    // chunk handling
    while (size > 0) {
        if (calldata->received_size > calldata->expected_size) {
            return false;
        }
        cpy_length = MIN(size, (sizeof(calldata->chunk) - calldata->chunk_size));
        memcpy(&calldata->chunk[calldata->chunk_size], buffer, cpy_length);
        calldata->chunk_size += cpy_length;
        if (calldata->chunk_size == CALLDATA_CHUNK_SIZE) {
            if (!compress_chunk(calldata)) {
                return false;
            }
            calldata->chunk_size = 0;
        }
        buffer += cpy_length;
        size -= cpy_length;
        calldata->received_size += cpy_length;
    }
#ifdef HAVE_PRINTF
    if (calldata->received_size == calldata->expected_size) {
        // get allocated size
        size_t compressed_size = sizeof(*calldata);
        for (s_calldata_chunk *chunk = calldata->chunks; chunk != NULL;
             chunk = (s_calldata_chunk *) ((s_flist_node *) chunk)->next) {
            compressed_size += sizeof(*chunk);
            compressed_size += chunk->size;
        }

        PRINTF("calldata size went from %u to %u bytes with compression\n",
               calldata->received_size,
               compressed_size);
        calldata_dump(calldata);
    }
#endif
    return true;
}

// to be used as a \ref f_list_node_del
static void delete_calldata_chunk(s_calldata_chunk *node) {
    app_mem_free(node->buf);
    app_mem_free(node);
}

void calldata_delete(s_calldata *node) {
    flist_clear((s_flist_node **) &node->chunks, (f_list_node_del) &delete_calldata_chunk);
    app_mem_free(node);
}

static bool has_valid_calldata(const s_calldata *calldata) {
    if (calldata == NULL) {
        PRINTF("Error: no calldata!\n");
        return false;
    }
    if (calldata->received_size != calldata->expected_size) {
        PRINTF("Error: incomplete calldata!\n");
        return false;
    }
    return true;
}

const uint8_t *calldata_get_selector(const s_calldata *calldata) {
    if (!has_valid_calldata(calldata)) {
        return NULL;
    }
    return calldata->selector;
}

const uint8_t *calldata_get_chunk(s_calldata *calldata, int idx) {
    s_calldata_chunk *chunk;

    if (!has_valid_calldata(calldata) || (calldata->chunks == NULL)) {
        return NULL;
    }
    chunk = calldata->chunks;
    for (int i = 0; i < idx; ++i) {
        if (((s_flist_node *) chunk)->next == NULL) return NULL;
        chunk = (s_calldata_chunk *) ((s_flist_node *) chunk)->next;
    }
    if (!decompress_chunk(chunk, calldata->chunk)) return NULL;
    return calldata->chunk;
}

void calldata_dump(const s_calldata *calldata) {
#ifdef HAVE_PRINTF
    int i = 0;
    uint8_t buf[CALLDATA_CHUNK_SIZE];

    PRINTF("=== calldata at 0x%p ===\n", calldata);
    PRINTF("selector = 0x%.*h\n", sizeof(calldata->selector), calldata->selector);
    for (s_calldata_chunk *chunk = calldata->chunks; chunk != NULL;
         chunk = (s_calldata_chunk *) ((s_flist_node *) chunk)->next) {
        if (!decompress_chunk(chunk, buf)) break;
        PRINTF("[%02u] %.*h\n", i++, CALLDATA_CHUNK_SIZE, buf);
    }
    PRINTF("========================\n");
#else
    (void) calldata;
#endif
}
