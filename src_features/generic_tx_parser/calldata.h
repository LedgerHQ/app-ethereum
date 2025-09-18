#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

#define CALLDATA_SELECTOR_SIZE 4
#define CALLDATA_CHUNK_SIZE    32

typedef enum {
    CHUNK_STRIP_LEFT = 0,
    CHUNK_STRIP_RIGHT = 1,
} e_chunk_strip_dir;

typedef struct {
    s_flist_node _list;
    e_chunk_strip_dir dir : 1;
    uint8_t size : 7;
    uint8_t *buf;
} s_calldata_chunk;

typedef struct {
    size_t expected_size;
    size_t received_size;

    uint8_t selector[CALLDATA_SELECTOR_SIZE];
    s_calldata_chunk *chunks;

    uint8_t chunk[CALLDATA_CHUNK_SIZE];
    size_t chunk_size;
} s_calldata;

s_calldata *calldata_init(size_t size, const uint8_t selector[CALLDATA_SELECTOR_SIZE]);
bool calldata_set_selector(s_calldata *calldata, const uint8_t selector[CALLDATA_SELECTOR_SIZE]);
bool calldata_append(s_calldata *calldata, const uint8_t *buffer, size_t size);
void calldata_delete(s_calldata *node);
const uint8_t *calldata_get_selector(const s_calldata *calldata);
const uint8_t *calldata_get_chunk(s_calldata *calldata, int idx);
void calldata_dump(const s_calldata *calldata);
