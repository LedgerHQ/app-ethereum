/**
 * @file test_data_path.c
 * @brief Unit tests for the calldata-path engine at
 *        src/features/generic_tx_parser/gtp_data_path.c.
 *
 * The data path describes how to walk ABI-encoded calldata to reach a
 * field. It is built from five element types — TUPLE, ARRAY, REF,
 * LEAF, SLICE — and is consumed in two phases:
 *   - handle_data_path_struct(): TLV-parses the path description into
 *     an s_data_path of (type, args) elements, capped at
 *     PATH_MAX_SIZE,
 *   - data_path_get(): walks the path against the live calldata and
 *     produces a parsed-value collection that downstream GCS
 *     formatters render.
 *
 * Tests cover:
 *   - TLV happy paths and the size limit at the common handler,
 *   - REF/LEAF payload guards (REF must be empty; LEAF must carry a
 *     valid e_path_leaf_type),
 *   - data_path_get on simple LEAF(STATIC) and SLICE shapes against a
 *     stubbed calldata,
 *   - path_slice boundary check (start >= end rejected),
 *   - data_path_cleanup frees the per-element ptr.
 *
 * The path_array runtime (with iteration count tracking) and path_ref
 * (with offset dereferencing) are deeper concerns and will get their
 * own coverage if needed — exercising them properly requires a
 * multi-chunk calldata fixture beyond this slice.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "gtp_data_path.h"
#include "gtp_path_array.h"
#include "gtp_path_slice.h"
#include "gtp_parsed_value.h"
#include "calldata.h"

// =============================================================================
// Wrapped collaborators — calldata_get_chunk / get_current_calldata
// =============================================================================

static s_calldata g_fake_calldata;
s_calldata *get_current_calldata(void) {
    return &g_fake_calldata;
}

// Test fixture: an array of CHUNK_SIZE chunks indexed by idx. Tests set
// up the chunks they need before running.
#define MAX_FIXTURE_CHUNKS 8
static uint8_t g_chunks[MAX_FIXTURE_CHUNKS][CALLDATA_CHUNK_SIZE];
static bool g_chunk_present[MAX_FIXTURE_CHUNKS];

const uint8_t *calldata_get_chunk(s_calldata *calldata, uint32_t idx) {
    (void) calldata;
    if (idx >= MAX_FIXTURE_CHUNKS || !g_chunk_present[idx]) {
        return NULL;
    }
    return g_chunks[idx];
}

// =============================================================================
// Fixtures
// =============================================================================

static void reset(void) {
    memset(g_chunks, 0, sizeof(g_chunks));
    memset(g_chunk_present, 0, sizeof(g_chunk_present));
}

static void set_chunk(uint32_t idx, const uint8_t *bytes, size_t len) {
    TEST_ASSERT_TRUE(idx < MAX_FIXTURE_CHUNKS);
    TEST_ASSERT_TRUE(len <= CALLDATA_CHUNK_SIZE);
    memset(g_chunks[idx], 0, CALLDATA_CHUNK_SIZE);
    memcpy(g_chunks[idx] + (CALLDATA_CHUNK_SIZE - len), bytes, len);
    g_chunk_present[idx] = true;
}

static bool run_tlv(const uint8_t *bytes, size_t size, s_data_path *path) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_data_path_context ctx = {.data_path = path};
    return handle_data_path_struct(&buf, &ctx);
}

// =============================================================================
// TLV layer
// =============================================================================

void test_tlv_happy_path_all_five_element_types(void) {
    const uint8_t bytes[] = {
        0x00,
        0x01,
        0x01,  // VERSION = 1
        0x01,
        0x02,
        0x00,
        0x20,  // TUPLE (value = 32)
        // ARRAY (delegated to path_array parser — TLV-empty so has_start/end stay false)
        0x02,
        0x00,
        0x03,
        0x00,  // REF — must be empty
        0x04,
        0x01,
        0x03,  // LEAF type = LEAF_TYPE_STATIC
        0x05,
        0x00,  // SLICE (delegated — empty)
    };
    s_data_path path = {0};
    TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &path));
    TEST_ASSERT_EQUAL(path.version, 1);
    TEST_ASSERT_EQUAL(path.size, 5);
    TEST_ASSERT_EQUAL(path.elements[0].type, ELEMENT_TYPE_TUPLE);
    TEST_ASSERT_EQUAL(path.elements[0].tuple.value, 32);
    TEST_ASSERT_EQUAL(path.elements[1].type, ELEMENT_TYPE_ARRAY);
    TEST_ASSERT_EQUAL(path.elements[2].type, ELEMENT_TYPE_REF);
    TEST_ASSERT_EQUAL(path.elements[3].type, ELEMENT_TYPE_LEAF);
    TEST_ASSERT_EQUAL(path.elements[3].leaf.type, LEAF_TYPE_STATIC);
    TEST_ASSERT_EQUAL(path.elements[4].type, ELEMENT_TYPE_SLICE);
}

void test_tlv_ref_with_payload_rejected(void) {
    // REF must have an empty payload; any byte is a hard reject.
    const uint8_t bytes[] = {0x03, 0x01, 0x00};
    s_data_path path = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &path));
}

void test_tlv_leaf_invalid_type_rejected(void) {
    // 0x99 is not one of LEAF_TYPE_{ARRAY,TUPLE,STATIC,DYNAMIC}
    const uint8_t bytes[] = {0x04, 0x01, 0x99};
    s_data_path path = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, sizeof(bytes), &path));
}

void test_tlv_leaf_each_valid_type_accepted(void) {
    const uint8_t types[] = {LEAF_TYPE_ARRAY, LEAF_TYPE_TUPLE, LEAF_TYPE_STATIC, LEAF_TYPE_DYNAMIC};
    for (size_t i = 0; i < sizeof(types); ++i) {
        s_data_path path = {0};
        const uint8_t bytes[] = {0x04, 0x01, types[i]};
        TEST_ASSERT_TRUE(run_tlv(bytes, sizeof(bytes), &path));
        TEST_ASSERT_EQUAL(path.size, 1);
        TEST_ASSERT_EQUAL(path.elements[0].leaf.type, types[i]);
    }
}

void test_tlv_path_max_size_enforced(void) {
    // Build PATH_MAX_SIZE + 1 = 17 TUPLE entries — the common handler
    // must reject the last one.
    uint8_t bytes[64];
    size_t off = 0;
    for (int i = 0; i <= PATH_MAX_SIZE; ++i) {
        bytes[off++] = 0x01;
        bytes[off++] = 0x02;
        bytes[off++] = 0x00;
        bytes[off++] = 0x01;
    }
    s_data_path path = {0};
    TEST_ASSERT_FALSE(run_tlv(bytes, off, &path));
}

// =============================================================================
// data_path_get — simple shapes
// =============================================================================

void test_data_path_get_empty_path_returns_true(void) {
    s_data_path path = {0};  // size == 0
    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    TEST_ASSERT_EQUAL(collec.size, 0);
}

void test_data_path_get_static_leaf_copies_chunk(void) {
    // The path is a single LEAF(STATIC). data_path_get must allocate
    // CALLDATA_CHUNK_SIZE bytes and copy chunk[0] into them.
    static const uint8_t chunk[] = {0xCA, 0xFE, 0xBA, 0xBE};
    set_chunk(0, chunk, sizeof(chunk));

    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = ELEMENT_TYPE_LEAF;
    path.elements[0].leaf.type = LEAF_TYPE_STATIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    TEST_ASSERT_EQUAL(collec.size, 1);
    TEST_ASSERT_EQUAL(collec.value[0].length, CALLDATA_CHUNK_SIZE);
    // Right-aligned: last 4 bytes match `chunk`, the rest is zero.
    static const uint8_t expected[CALLDATA_CHUNK_SIZE] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    0,    0,    0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xCA, 0xFE, 0xBA, 0xBE,
    };
    TEST_ASSERT_EQUAL_MEMORY(collec.value[0].ptr, expected, CALLDATA_CHUNK_SIZE);
    data_path_cleanup(&collec);
}

void test_data_path_get_slice_trims_collection(void) {
    // LEAF(STATIC) → 32-byte value. Then SLICE [4, 12) → length=8, ptr
    // advanced by 4, offset=4.
    static const uint8_t bytes[CALLDATA_CHUNK_SIZE] = {
        0, 0, 0, 0, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0, 0, 0, 0,
        0, 0, 0, 0, 0,    0,    0,    0,    0,    0,    0,    0,    0, 0, 0, 0,
    };
    memcpy(g_chunks[0], bytes, CALLDATA_CHUNK_SIZE);
    g_chunk_present[0] = true;

    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_LEAF;
    path.elements[0].leaf.type = LEAF_TYPE_STATIC;
    path.elements[1].type = ELEMENT_TYPE_SLICE;
    path.elements[1].slice.has_start = true;
    path.elements[1].slice.start = 4;
    path.elements[1].slice.has_end = true;
    path.elements[1].slice.end = 12;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    TEST_ASSERT_EQUAL(collec.size, 1);
    TEST_ASSERT_EQUAL(collec.value[0].length, 8);
    TEST_ASSERT_EQUAL(collec.value[0].offset, 4);
    static const uint8_t expected[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    TEST_ASSERT_EQUAL_MEMORY(collec.value[0].ptr, expected, 8);
    data_path_cleanup(&collec);
}

void test_data_path_get_slice_start_ge_end_rejected(void) {
    static const uint8_t chunk[] = {0};
    set_chunk(0, chunk, sizeof(chunk));

    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_LEAF;
    path.elements[0].leaf.type = LEAF_TYPE_STATIC;
    path.elements[1].type = ELEMENT_TYPE_SLICE;
    path.elements[1].slice.has_start = true;
    path.elements[1].slice.start = 10;
    path.elements[1].slice.has_end = true;
    path.elements[1].slice.end = 10;  // start == end → reject

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

void test_data_path_get_leaf_missing_chunk_returns_false(void) {
    // Path asks for LEAF(DYNAMIC) but the calldata stub has no chunk
    // at offset 0 → must propagate false without dereferencing.
    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = ELEMENT_TYPE_LEAF;
    path.elements[0].leaf.type = LEAF_TYPE_DYNAMIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

void test_data_path_get_leaf_invalid_type_rejected_at_runtime(void) {
    // LEAF_TYPE_TUPLE / LEAF_TYPE_ARRAY are accepted by the TLV layer
    // but not yet implemented at runtime — they fall into the default
    // case of path_leaf.
    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = ELEMENT_TYPE_LEAF;
    path.elements[0].leaf.type = LEAF_TYPE_TUPLE;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

// =============================================================================
// data_path_cleanup — frees allocated leaves
// =============================================================================

void test_data_path_cleanup_frees_allocated_ptr(void) {
    // Allocate a buffer matching the convention used by path_leaf and
    // assert that cleanup does not crash. The malloc/free contract is
    // exercised via the existing mem_utils wrapper in mock.c.
    s_parsed_value_collection collec = {0};
    collec.size = 1;
    collec.value[0].ptr = (uint8_t *) malloc(16);
    collec.value[0].length = 16;
    collec.value[0].offset = 0;
    data_path_cleanup(&collec);
    // No leak / crash → test passes implicitly.
}

// =============================================================================
// path_array runtime — multi-iteration walk
// =============================================================================
//
// path_array reads a u16 array_size from chunk[offset], then bumps the
// outer do-while loop to walk the path once per element. arrays_update
// counts down each iteration; when the counter reaches zero, depth--
// and the loop exits.
//
// Chunk layout for an array_size of N: 30 zero bytes followed by the
// 2-byte big-endian N value (the high bytes are validated by
// is_zeroes_buffer).

static void put_array_size_chunk(uint32_t idx, uint16_t array_size) {
    TEST_ASSERT_TRUE(idx < MAX_FIXTURE_CHUNKS);
    memset(g_chunks[idx], 0, CALLDATA_CHUNK_SIZE);
    g_chunks[idx][CALLDATA_CHUNK_SIZE - 2] = (uint8_t) (array_size >> 8);
    g_chunks[idx][CALLDATA_CHUNK_SIZE - 1] = (uint8_t) (array_size & 0xFF);
    g_chunk_present[idx] = true;
}

void test_path_array_size_one_iterates_once(void) {
    // chunk[0] holds array_size=1; chunk[1] is the element body (STATIC
    // LEAF reads chunk_size bytes).
    put_array_size_chunk(0, 1);
    static const uint8_t body[] = {0xAB, 0xCD};
    set_chunk(1, body, sizeof(body));

    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_ARRAY;
    path.elements[0].array.weight = 1;  // single-chunk elements
    path.elements[1].type = ELEMENT_TYPE_LEAF;
    path.elements[1].leaf.type = LEAF_TYPE_STATIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    // One iteration produced one collection entry.
    TEST_ASSERT_EQUAL(collec.size, 1);
    data_path_cleanup(&collec);
}

void test_path_array_size_two_iterates_twice(void) {
    put_array_size_chunk(0, 2);
    static const uint8_t e0[] = {0x11};
    static const uint8_t e1[] = {0x22};
    set_chunk(1, e0, sizeof(e0));
    set_chunk(2, e1, sizeof(e1));

    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_ARRAY;
    path.elements[0].array.weight = 1;
    path.elements[1].type = ELEMENT_TYPE_LEAF;
    path.elements[1].leaf.type = LEAF_TYPE_STATIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    TEST_ASSERT_EQUAL(collec.size, 2);
    data_path_cleanup(&collec);
}

void test_path_array_slice_start_end(void) {
    // array_size=5, slice [1, 3) → 2 iterations on chunks 2 and 3.
    put_array_size_chunk(0, 5);
    // weight=1, idx=start..end-1; chunk fetched at offset = 1 + idx * weight
    static const uint8_t e1[] = {0x11};
    static const uint8_t e2[] = {0x22};
    set_chunk(1 + 1, e1, sizeof(e1));  // idx=1
    set_chunk(1 + 2, e2, sizeof(e2));  // idx=2

    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_ARRAY;
    path.elements[0].array.weight = 1;
    path.elements[0].array.has_start = true;
    path.elements[0].array.start = 1;
    path.elements[0].array.has_end = true;
    path.elements[0].array.end = 3;
    path.elements[1].type = ELEMENT_TYPE_LEAF;
    path.elements[1].leaf.type = LEAF_TYPE_STATIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    TEST_ASSERT_EQUAL(collec.size, 2);
    data_path_cleanup(&collec);
}

void test_path_array_end_le_start_rejected(void) {
    put_array_size_chunk(0, 5);

    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = ELEMENT_TYPE_ARRAY;
    path.elements[0].array.weight = 1;
    path.elements[0].array.has_start = true;
    path.elements[0].array.start = 3;
    path.elements[0].array.has_end = true;
    path.elements[0].array.end = 3;  // end == start → reject

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

void test_path_array_size_chunk_missing_rejected(void) {
    // chunk[0] not set → path_array's calldata_get_chunk returns NULL.
    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = ELEMENT_TYPE_ARRAY;
    path.elements[0].array.weight = 1;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

void test_path_array_non_zero_high_bytes_rejected(void) {
    // First byte non-zero — is_zeroes_buffer fails before reading size.
    memset(g_chunks[0], 0xFF, CALLDATA_CHUNK_SIZE);
    g_chunk_present[0] = true;

    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = ELEMENT_TYPE_ARRAY;
    path.elements[0].array.weight = 1;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

// =============================================================================
// path_ref runtime
// =============================================================================

void test_path_ref_dereferences_offset(void) {
    // chunk[0] holds raw_offset = 1 * CALLDATA_CHUNK_SIZE = 32. After
    // path_ref, *offset is 1; the subsequent LEAF reads chunk[1].
    memset(g_chunks[0], 0, CALLDATA_CHUNK_SIZE);
    g_chunks[0][CALLDATA_CHUNK_SIZE - 1] = CALLDATA_CHUNK_SIZE;  // 0x20
    g_chunk_present[0] = true;
    static const uint8_t target[] = {0xAB};
    set_chunk(1, target, sizeof(target));

    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_REF;
    path.elements[1].type = ELEMENT_TYPE_LEAF;
    path.elements[1].leaf.type = LEAF_TYPE_STATIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    TEST_ASSERT_EQUAL(collec.size, 1);
    data_path_cleanup(&collec);
}

void test_path_ref_unaligned_offset_rejected(void) {
    // raw_offset = 33 is not a multiple of CALLDATA_CHUNK_SIZE.
    memset(g_chunks[0], 0, CALLDATA_CHUNK_SIZE);
    g_chunks[0][CALLDATA_CHUNK_SIZE - 1] = 33;
    g_chunk_present[0] = true;

    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = ELEMENT_TYPE_REF;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

// =============================================================================
// path_tuple runtime
// =============================================================================

void test_path_tuple_advances_offset(void) {
    // TUPLE(value=2) jumps 2 chunks then LEAF(STATIC) reads chunk[2].
    static const uint8_t body[] = {0xFE, 0xED};
    set_chunk(2, body, sizeof(body));

    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_TUPLE;
    path.elements[0].tuple.value = 2;
    path.elements[1].type = ELEMENT_TYPE_LEAF;
    path.elements[1].leaf.type = LEAF_TYPE_STATIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_TRUE(data_path_get(&path, &collec));
    TEST_ASSERT_EQUAL(collec.size, 1);
    data_path_cleanup(&collec);
}

// =============================================================================
// Tail coverage — security gates that the base suite skipped.
// =============================================================================

// path_array enforces passes <= UINT8_MAX (line 286 in gtp_data_path.c).
// An array_size larger than 255 with no slice bounds must be rejected
// before allocating a passes_remaining slot.
void test_path_array_size_above_uint8_max_rejected(void) {
    put_array_size_chunk(0, 256);  // > UINT8_MAX
    s_data_path path = {0};
    path.size = 2;
    path.elements[0].type = ELEMENT_TYPE_ARRAY;
    path.elements[0].array.weight = 1;
    path.elements[1].type = ELEMENT_TYPE_LEAF;
    path.elements[1].leaf.type = LEAF_TYPE_STATIC;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
    data_path_cleanup(&collec);
}

// data_path_get default switch — element with an unknown type must
// reject (line 342-343 in gtp_data_path.c). Construct a path whose
// only element has a type outside the enum.
void test_data_path_get_unknown_element_type_rejected(void) {
    s_data_path path = {0};
    path.size = 1;
    path.elements[0].type = (e_path_element_type) 0xFF;

    s_parsed_value_collection collec = {0};
    TEST_ASSERT_FALSE(data_path_get(&path, &collec));
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    reset();
}
void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tlv_happy_path_all_five_element_types);
    RUN_TEST(test_tlv_ref_with_payload_rejected);
    RUN_TEST(test_tlv_leaf_invalid_type_rejected);
    RUN_TEST(test_tlv_leaf_each_valid_type_accepted);
    RUN_TEST(test_tlv_path_max_size_enforced);
    RUN_TEST(test_data_path_get_empty_path_returns_true);
    RUN_TEST(test_data_path_get_static_leaf_copies_chunk);
    RUN_TEST(test_data_path_get_slice_trims_collection);
    RUN_TEST(test_data_path_get_slice_start_ge_end_rejected);
    RUN_TEST(test_data_path_get_leaf_missing_chunk_returns_false);
    RUN_TEST(test_data_path_get_leaf_invalid_type_rejected_at_runtime);
    RUN_TEST(test_data_path_cleanup_frees_allocated_ptr);
    RUN_TEST(test_path_array_size_one_iterates_once);
    RUN_TEST(test_path_array_size_two_iterates_twice);
    RUN_TEST(test_path_array_slice_start_end);
    RUN_TEST(test_path_array_end_le_start_rejected);
    RUN_TEST(test_path_array_size_chunk_missing_rejected);
    RUN_TEST(test_path_array_non_zero_high_bytes_rejected);
    RUN_TEST(test_path_ref_dereferences_offset);
    RUN_TEST(test_path_ref_unaligned_offset_rejected);
    RUN_TEST(test_path_tuple_advances_offset);
    RUN_TEST(test_path_array_size_above_uint8_max_rejected);
    RUN_TEST(test_data_path_get_unknown_element_type_rejected);
    return UNITY_END();
}
