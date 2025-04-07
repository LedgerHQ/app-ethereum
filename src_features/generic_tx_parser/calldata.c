#ifdef HAVE_GENERIC_TX_PARSER

#include <string.h>
#include "os_math.h"  // MIN
#include "calldata.h"
#include "os_print.h"
#include "mem.h"
#include "mem_utils.h"

typedef struct {
    uint8_t selector[CALLDATA_SELECTOR_SIZE];
    // chunk_info (8 bit):
    // ........
    //   ^^^^^^ byte size
    //  ^ unused
    // ^ strip direction : 0 LEFT, 1 RIGHT
    //
    // chunk_info (1) | chunk (n) | chunk info (1) | chunk (n) | ...
    uint8_t *chunks;
    size_t chunks_size;
    size_t expected_size;
    size_t received_size;
    uint8_t chunk[CALLDATA_CHUNK_SIZE];
    size_t chunk_size;
} s_calldata;

static s_calldata *g_calldata = NULL;
static uint8_t g_calldata_alignment;

typedef enum {
    CHUNK_STRIP_LEFT = 0,
    CHUNK_STRIP_RIGHT = 1,
} e_chunk_strip_dir;

#define CHUNK_INFO_SIZE_MASK   0x3f
#define CHUNK_INFO_SIZE_OFFSET 0

#define CHUNK_INFO_DIR_MASK   0x01
#define CHUNK_INFO_DIR_OFFSET 7

#define CHUNK_INFO_SIZE(v) \
    ((v & (CHUNK_INFO_SIZE_MASK << CHUNK_INFO_SIZE_OFFSET)) >> CHUNK_INFO_SIZE_OFFSET)
#define CHUNK_INFO_DIR(v) \
    ((v & (CHUNK_INFO_DIR_MASK << CHUNK_INFO_DIR_OFFSET)) >> CHUNK_INFO_DIR_OFFSET)

typedef uint8_t chunk_info_t;

bool calldata_init(size_t size) {
    if (g_calldata != NULL) {
        calldata_cleanup();
    }
    g_calldata_alignment = mem_align(__alignof__(*g_calldata));
    if ((g_calldata = mem_alloc(sizeof(*g_calldata))) == NULL) {
        return false;
    }
    explicit_bzero(g_calldata, sizeof(*g_calldata));
    g_calldata->expected_size = size;
    return true;
}

static bool compress_chunk(s_calldata *calldata) {
    uint8_t strip_left = 0;
    uint8_t strip_right = 0;
    chunk_info_t chunk_info = 0;
    e_chunk_strip_dir strip_dir;
    uint8_t stripped_size;
    uint8_t start_idx;
    uint8_t *ptr;

    for (int i = 0; (i < CALLDATA_CHUNK_SIZE) && (calldata->chunk[i] == 0x00); ++i) {
        strip_left += 1;
    }
    for (int i = CALLDATA_CHUNK_SIZE - 1; (i >= 0) && (calldata->chunk[i] == 0x00); --i) {
        strip_right += 1;
    }

    if (strip_left >= strip_right) {
        strip_dir = CHUNK_STRIP_LEFT;
        stripped_size = CALLDATA_CHUNK_SIZE - strip_left;
        start_idx = strip_left;
    } else {
        strip_dir = CHUNK_STRIP_RIGHT;
        stripped_size = CALLDATA_CHUNK_SIZE - strip_right;
        start_idx = 0;
    }
    chunk_info |= strip_dir << CHUNK_INFO_DIR_OFFSET;
    chunk_info |= stripped_size << CHUNK_INFO_SIZE_OFFSET;
    if ((ptr = mem_alloc(sizeof(chunk_info) + stripped_size)) == NULL) {
        return false;
    }
    if (calldata->chunks == NULL) {
        calldata->chunks = ptr;
    } else {
        if ((calldata->chunks + calldata->chunks_size) != ptr) {
            // something was allocated in between two compressed chunks
            return false;
        }
    }
    calldata->chunks_size += sizeof(chunk_info) + stripped_size;
    memcpy(ptr, &chunk_info, sizeof(chunk_info));
    memcpy(ptr + sizeof(chunk_info), calldata->chunk + start_idx, stripped_size);
    return true;
}

static bool decompress_chunk(s_calldata *calldata, size_t offset) {
    chunk_info_t chunk_info = calldata->chunks[offset];
    const uint8_t *compressed_chunk = &calldata->chunks[offset + sizeof(chunk_info)];
    size_t diff;

    diff = CALLDATA_CHUNK_SIZE - CHUNK_INFO_SIZE(chunk_info);
    if (CHUNK_INFO_DIR(chunk_info) == CHUNK_STRIP_LEFT) {
        explicit_bzero(calldata->chunk, diff);
        memcpy(&calldata->chunk[diff], compressed_chunk, CHUNK_INFO_SIZE(chunk_info));
    } else {
        memcpy(calldata->chunk, compressed_chunk, CHUNK_INFO_SIZE(chunk_info));
        explicit_bzero(&calldata->chunk[CHUNK_INFO_SIZE(chunk_info)], diff);
    }
    return true;
}

bool calldata_append(const uint8_t *buffer, size_t size) {
    uint8_t cpy_length;

    if (g_calldata == NULL) return false;
    if ((g_calldata->received_size + size) > g_calldata->expected_size) {
        calldata_cleanup();
        return false;
    }

    // selector handling
    if (g_calldata->chunks == NULL) {
        if (size < CALLDATA_SELECTOR_SIZE) {
            // somehow getting an imcomplete selector
            calldata_cleanup();
            return false;
        }
        memcpy(g_calldata->selector, buffer, CALLDATA_SELECTOR_SIZE);
        buffer += CALLDATA_SELECTOR_SIZE;
        size -= CALLDATA_SELECTOR_SIZE;
        g_calldata->received_size += CALLDATA_SELECTOR_SIZE;
    }

    // chunk handling
    while (size > 0) {
        if (g_calldata->received_size > g_calldata->expected_size) {
            calldata_cleanup();
            return false;
        }
        cpy_length = MIN(size, (sizeof(g_calldata->chunk) - g_calldata->chunk_size));
        memcpy(&g_calldata->chunk[g_calldata->chunk_size], buffer, cpy_length);
        g_calldata->chunk_size += cpy_length;
        if (g_calldata->chunk_size == CALLDATA_CHUNK_SIZE) {
            if (!compress_chunk(g_calldata)) {
                calldata_cleanup();
                return false;
            }
            g_calldata->chunk_size = 0;
        }
        buffer += cpy_length;
        size -= cpy_length;
        g_calldata->received_size += cpy_length;
    }
    if (g_calldata->received_size == g_calldata->expected_size) {
        PRINTF("calldata reduced by ~%u%% with compression (%u -> %u bytes)\n",
               100 - (100 * (CALLDATA_SELECTOR_SIZE + g_calldata->chunks_size) /
                      g_calldata->received_size),
               g_calldata->received_size,
               CALLDATA_SELECTOR_SIZE + g_calldata->chunks_size);
    }
    return true;
}

void calldata_cleanup(void) {
    mem_dealloc(g_calldata->chunks_size);
    mem_dealloc(sizeof(*g_calldata));
    g_calldata = NULL;
    mem_dealloc(g_calldata_alignment);
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

const uint8_t *calldata_get_selector(void) {
    if (!has_valid_calldata(g_calldata)) {
        return NULL;
    }
    return g_calldata->selector;
}

const uint8_t *calldata_get_chunk(int idx) {
    size_t offset = 0;

    if (!has_valid_calldata(g_calldata) || (g_calldata->chunks == NULL)) {
        return NULL;
    }
    for (int i = 0; i < idx; ++i) {
        if (offset > g_calldata->chunks_size) return NULL;
        offset += sizeof(chunk_info_t) + CHUNK_INFO_SIZE(g_calldata->chunks[offset]);
    }
    if (!decompress_chunk(g_calldata, offset)) return NULL;
    return g_calldata->chunk;
}

#endif  // HAVE_GENERIC_TX_PARSER
