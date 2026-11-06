/**
 * @file test_eip712_v2_path.c
 * @brief Unit tests for the EIP712_PATH struct at
 *        src/features/sign_message_eip712/eip712_v2_path.c
 *
 * An EIP712_PATH is the location of a value in the message's value tree: field indices to go down
 * through structs, and slices to select the elements of the array dimensions on the way. Tests
 * parse the wire form, then follow paths on a message built with the typed data API:
 *
 *   Item  { uint256 price; address token; }
 *   Order { address owner; Item[] items; uint8[2][] grid; bytes blob; string name; }
 *
 * with three items, a grid of [[1, 2], [3, 4]] and a blob of 6 bytes.
 */

#include "unity.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "eip712_v2_path.h"
#include "typed_data.h"

// the hash pass is not what these tests are about
bool value_hash_pass(const s_eip712_impl *impl) {
    (void) impl;
    return true;
}

bool value_hash_domain(const s_eip712_impl *impl, uint8_t out[CX_KECCAK_256_SIZE]) {
    (void) impl;
    (void) out;
    return true;
}

bool value_hash_domain_with_contract(const s_eip712_impl *impl,
                                     const uint8_t contract[ADDRESS_LENGTH],
                                     uint8_t out[CX_KECCAK_256_SIZE]) {
    (void) impl;
    (void) contract;
    (void) out;
    return false;
}

// =============================================================================
// Message
// =============================================================================

#define ITEM_FIELD_PRICE 0
#define ITEM_FIELD_TOKEN 1

#define ORDER_FIELD_OWNER 0
#define ORDER_FIELD_ITEMS 1
#define ORDER_FIELD_GRID  2
#define ORDER_FIELD_BLOB  3
#define ORDER_FIELD_NAME  4

static const uint8_t k_blob[] = {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5};

static s_struct_712_field *new_field(e_type type, const char *name) {
    s_struct_712_field *field = td_create_field_def(type);

    TEST_ASSERT_NOT_NULL(field);
    TEST_ASSERT_TRUE(td_field_set_key_name(field, (const uint8_t *) name, strlen(name)));
    return field;
}

static s_struct_712_field *new_sized_field(e_type type, uint8_t size, const char *name) {
    s_struct_712_field *field = new_field(type, name);

    TEST_ASSERT_TRUE(td_field_set_type_size(field, size));
    return field;
}

static s_struct_712_field *new_struct_field(const char *struct_name, const char *name) {
    s_struct_712_field *field = new_field(TYPE_STRUCT, name);

    TEST_ASSERT_TRUE(
        td_field_set_struct_name(field, (const uint8_t *) struct_name, strlen(struct_name)));
    return field;
}

static void add_field(s_struct_712 *def, s_struct_712_field *field) {
    TEST_ASSERT_TRUE(td_add_struct_field_def(def, field));
}

static s_struct_712 *new_struct_def(const char *name) {
    s_struct_712 *def = td_create_struct_def((const uint8_t *) name, strlen(name));

    TEST_ASSERT_NOT_NULL(def);
    return def;
}

static void build_schema(void) {
    s_struct_712 *item = new_struct_def("Item");
    s_struct_712 *order = new_struct_def("Order");
    s_struct_712_field *items = new_struct_field("Item", "items");
    s_struct_712_field *grid = new_sized_field(TYPE_SOL_UINT, 1, "grid");

    add_field(item, new_sized_field(TYPE_SOL_UINT, 32, "price"));
    add_field(item, new_field(TYPE_SOL_ADDRESS, "token"));
    TEST_ASSERT_TRUE(td_add_struct_def(item));

    add_field(order, new_field(TYPE_SOL_ADDRESS, "owner"));
    TEST_ASSERT_TRUE(td_field_set_array_level_count(items, 1));
    TEST_ASSERT_TRUE(td_field_set_array_level(items, 0, ARRAY_DYNAMIC, 0));
    add_field(order, items);
    // innermost dimension first: uint8[2][]
    TEST_ASSERT_TRUE(td_field_set_array_level_count(grid, 2));
    TEST_ASSERT_TRUE(td_field_set_array_level(grid, 0, ARRAY_FIXED_SIZE, 2));
    TEST_ASSERT_TRUE(td_field_set_array_level(grid, 1, ARRAY_DYNAMIC, 0));
    add_field(order, grid);
    add_field(order, new_field(TYPE_SOL_BYTES_DYN, "blob"));
    add_field(order, new_field(TYPE_SOL_STRING, "name"));
    TEST_ASSERT_TRUE(td_add_struct_def(order));
}

static void add_leaf(s_struct_712_value *parent,
                     const s_struct_712_field *field,
                     const uint8_t *data,
                     size_t length) {
    s_struct_712_value *leaf = td_create_leaf(field, length);

    TEST_ASSERT_NOT_NULL(leaf);
    TEST_ASSERT_TRUE(td_leaf_write(leaf, 0, data, length));
    TEST_ASSERT_TRUE(td_append_child(parent, leaf));
}

static const s_struct_712_field *field_at(const s_struct_712 *def, size_t index) {
    const s_struct_712_field *field = def->fields;

    for (size_t i = 0; i < index; ++i) {
        field = (const s_struct_712_field *) ((const flist_node_t *) field)->next;
    }
    return field;
}

/**
 * Build the message
 *
 * @param[in] item_count number of items in the order
 */
static void build_message(size_t item_count) {
    const s_struct_712 *item_def = td_find_struct("Item");
    const s_struct_712 *order_def = td_find_struct("Order");
    s_struct_712_value *order = td_create_struct(order_def);
    s_struct_712_value *items = td_create_array(field_at(order_def, ORDER_FIELD_ITEMS));
    s_struct_712_value *grid = td_create_array(field_at(order_def, ORDER_FIELD_GRID));
    uint8_t owner[ADDRESS_LENGTH];

    memset(owner, 0xAA, sizeof(owner));
    add_leaf(order, field_at(order_def, ORDER_FIELD_OWNER), owner, sizeof(owner));

    for (size_t i = 0; i < item_count; ++i) {
        s_struct_712_value *item = td_create_struct(item_def);
        uint8_t price[INT256_LENGTH] = {0};
        uint8_t token[ADDRESS_LENGTH];

        price[INT256_LENGTH - 1] = (uint8_t) (i + 1);
        memset(token, 0x10 + i, sizeof(token));
        add_leaf(item, field_at(item_def, ITEM_FIELD_PRICE), price, sizeof(price));
        add_leaf(item, field_at(item_def, ITEM_FIELD_TOKEN), token, sizeof(token));
        TEST_ASSERT_TRUE(td_append_child(items, item));
    }
    TEST_ASSERT_TRUE(td_append_child(order, items));

    // [[1, 2], [3, 4]]
    for (uint8_t row = 0; row < 2; ++row) {
        s_struct_712_value *inner = td_create_array(field_at(order_def, ORDER_FIELD_GRID));

        for (uint8_t col = 0; col < 2; ++col) {
            uint8_t value = (uint8_t) ((row * 2) + col + 1);

            add_leaf(inner, field_at(order_def, ORDER_FIELD_GRID), &value, sizeof(value));
        }
        TEST_ASSERT_TRUE(td_append_child(grid, inner));
    }
    TEST_ASSERT_TRUE(td_append_child(order, grid));

    add_leaf(order, field_at(order_def, ORDER_FIELD_BLOB), k_blob, sizeof(k_blob));
    add_leaf(order, field_at(order_def, ORDER_FIELD_NAME), (const uint8_t *) "Bob", 3);
    TEST_ASSERT_TRUE(td_set_message(order));
}

// =============================================================================
// Path
// =============================================================================

#define NO_BOUND INT16_MIN

static s_slice_args make_slice(int16_t start, int16_t end) {
    return (s_slice_args) {.has_start = (start != NO_BOUND),
                           .start = (start != NO_BOUND) ? start : 0,
                           .has_end = (end != NO_BOUND),
                           .end = (end != NO_BOUND) ? end : 0};
}

static void add_field_step(s_eip712_path *path, uint8_t index) {
    path->elements[path->size].type = EIP712_ELEMENT_STRUCT_FIELD;
    path->elements[path->size].field_index = index;
    path->size += 1;
}

static void add_array_step(s_eip712_path *path, int16_t start, int16_t end) {
    path->elements[path->size].type = EIP712_ELEMENT_ARRAY_SLICE;
    path->elements[path->size].slice = make_slice(start, end);
    path->size += 1;
}

static void add_leaf_step(s_eip712_path *path, int16_t start, int16_t end) {
    path->elements[path->size].type = EIP712_ELEMENT_LEAF_SLICE;
    path->elements[path->size].slice = make_slice(start, end);
    path->size += 1;
}

static bool parse(const uint8_t *bytes, size_t size, s_eip712_path *path) {
    buffer_t buf = {.ptr = (uint8_t *) bytes, .size = size, .offset = 0};
    s_eip712_path_context ctx = {.path = path};

    return handle_eip712_path_struct(&buf, &ctx);
}

// =============================================================================
// Parsing
// =============================================================================

#define VERSION_1    0x00, 0x01, 0x01
#define FIELD(index) 0x01, 0x01, (index)

void test_parse_field_steps(void) {
    static const uint8_t bytes[] = {VERSION_1, FIELD(1), FIELD(0)};
    s_eip712_path path;

    TEST_ASSERT_TRUE(parse(bytes, sizeof(bytes), &path));
    TEST_ASSERT_EQUAL(2, path.size);
    TEST_ASSERT_EQUAL(EIP712_ELEMENT_STRUCT_FIELD, path.elements[0].type);
    TEST_ASSERT_EQUAL(1, path.elements[0].field_index);
    TEST_ASSERT_EQUAL(EIP712_ELEMENT_STRUCT_FIELD, path.elements[1].type);
    TEST_ASSERT_EQUAL(0, path.elements[1].field_index);
}

// The order of the steps is the order of the descent
void test_parse_keeps_the_order_of_steps(void) {
    static const uint8_t bytes[] = {
        VERSION_1, FIELD(1), 0x02, 0x08, 0x01, 0x02, 0x00, 0x01, 0x02, 0x02, 0xFF, 0xFE,  // [1, -2)
        FIELD(0),  0x02,     0x00,                                                        // [:]
        0x03,      0x04,     0x01, 0x02, 0x00, 0x02,  // leaf bytes [2:]
    };
    s_eip712_path path;

    TEST_ASSERT_TRUE(parse(bytes, sizeof(bytes), &path));
    TEST_ASSERT_EQUAL(5, path.size);
    TEST_ASSERT_EQUAL(EIP712_ELEMENT_STRUCT_FIELD, path.elements[0].type);
    TEST_ASSERT_EQUAL(EIP712_ELEMENT_ARRAY_SLICE, path.elements[1].type);
    TEST_ASSERT_TRUE(path.elements[1].slice.has_start);
    TEST_ASSERT_EQUAL(1, path.elements[1].slice.start);
    TEST_ASSERT_TRUE(path.elements[1].slice.has_end);
    TEST_ASSERT_EQUAL(-2, path.elements[1].slice.end);
    TEST_ASSERT_EQUAL(EIP712_ELEMENT_STRUCT_FIELD, path.elements[2].type);
    TEST_ASSERT_EQUAL(EIP712_ELEMENT_ARRAY_SLICE, path.elements[3].type);
    TEST_ASSERT_FALSE(path.elements[3].slice.has_start);
    TEST_ASSERT_FALSE(path.elements[3].slice.has_end);
    TEST_ASSERT_EQUAL(EIP712_ELEMENT_LEAF_SLICE, path.elements[4].type);
    TEST_ASSERT_TRUE(path.elements[4].slice.has_start);
    TEST_ASSERT_EQUAL(2, path.elements[4].slice.start);
    TEST_ASSERT_FALSE(path.elements[4].slice.has_end);
}

void test_parse_refuses_a_missing_or_unsupported_version(void) {
    static const uint8_t no_version[] = {FIELD(0)};
    static const uint8_t version_0[] = {0x00, 0x01, 0x00, FIELD(0)};
    static const uint8_t version_2[] = {0x00, 0x01, 0x02, FIELD(0)};
    s_eip712_path path;

    TEST_ASSERT_FALSE(parse(no_version, sizeof(no_version), &path));
    TEST_ASSERT_FALSE(parse(version_0, sizeof(version_0), &path));
    TEST_ASSERT_FALSE(parse(version_2, sizeof(version_2), &path));
}

void test_parse_refuses_a_path_that_does_not_start_with_a_field(void) {
    static const uint8_t empty[] = {VERSION_1};
    static const uint8_t slice_first[] = {VERSION_1, 0x02, 0x00, FIELD(0)};
    s_eip712_path path;

    TEST_ASSERT_FALSE(parse(empty, sizeof(empty), &path));
    TEST_ASSERT_FALSE(parse(slice_first, sizeof(slice_first), &path));
}

void test_parse_refuses_steps_below_the_leaf_slice(void) {
    static const uint8_t field_after[] = {VERSION_1, FIELD(3), 0x03, 0x00, FIELD(0)};
    static const uint8_t slice_after[] = {VERSION_1, FIELD(3), 0x03, 0x00, 0x02, 0x00};
    static const uint8_t two_leaf_slices[] = {VERSION_1, FIELD(3), 0x03, 0x00, 0x03, 0x00};
    s_eip712_path path;

    TEST_ASSERT_FALSE(parse(field_after, sizeof(field_after), &path));
    TEST_ASSERT_FALSE(parse(slice_after, sizeof(slice_after), &path));
    TEST_ASSERT_FALSE(parse(two_leaf_slices, sizeof(two_leaf_slices), &path));
}

// The length of a bound gives its sign: one byte is 0 to 255, two bytes are two's complement
void test_parse_reads_one_byte_bounds_as_not_negative(void) {
    static const uint8_t bytes[] = {
        VERSION_1,
        FIELD(1),
        0x02,
        0x06,
        0x01,
        0x01,
        0xFF,
        0x02,
        0x01,
        0x05,  // [255, 5)
        FIELD(2),
        0x03,
        0x03,
        0x01,
        0x01,
        0xC8,  // leaf bytes [200:]
    };
    s_eip712_path path;

    TEST_ASSERT_TRUE(parse(bytes, sizeof(bytes), &path));
    TEST_ASSERT_EQUAL(255, path.elements[1].slice.start);
    TEST_ASSERT_EQUAL(5, path.elements[1].slice.end);
    TEST_ASSERT_EQUAL(200, path.elements[3].slice.start);
}

void test_parse_reads_one_byte_and_two_byte_bounds_alike(void) {
    static const uint8_t one_byte[] = {VERSION_1, FIELD(1), 0x02, 0x03, 0x01, 0x01, 0xFF};
    static const uint8_t two_bytes[] = {VERSION_1, FIELD(1), 0x02, 0x04, 0x01, 0x02, 0x00, 0xFF};
    static const uint8_t negative[] = {VERSION_1, FIELD(1), 0x02, 0x04, 0x01, 0x02, 0xFF, 0xFF};
    s_eip712_path one;
    s_eip712_path two;
    s_eip712_path minus_one;

    TEST_ASSERT_TRUE(parse(one_byte, sizeof(one_byte), &one));
    TEST_ASSERT_TRUE(parse(two_bytes, sizeof(two_bytes), &two));
    TEST_ASSERT_TRUE(parse(negative, sizeof(negative), &minus_one));
    TEST_ASSERT_EQUAL(two.elements[1].slice.start, one.elements[1].slice.start);
    TEST_ASSERT_EQUAL(-1, minus_one.elements[1].slice.start);
}

// A longer encoding of 2 would be read as 0
void test_parse_refuses_bounds_of_another_length(void) {
    static const uint8_t empty[] = {VERSION_1, FIELD(1), 0x02, 0x02, 0x01, 0x00};
    static const uint8_t three_bytes[] =
        {VERSION_1, FIELD(1), 0x02, 0x05, 0x01, 0x03, 0x00, 0x00, 0x02};
    static const uint8_t four_bytes[] =
        {VERSION_1, FIELD(1), 0x02, 0x06, 0x01, 0x04, 0x00, 0x00, 0x00, 0x02};
    s_eip712_path path;

    TEST_ASSERT_FALSE(parse(empty, sizeof(empty), &path));
    TEST_ASSERT_FALSE(parse(three_bytes, sizeof(three_bytes), &path));
    TEST_ASSERT_FALSE(parse(four_bytes, sizeof(four_bytes), &path));
}

void test_parse_refuses_a_repeated_bound(void) {
    static const uint8_t bytes[] =
        {VERSION_1, FIELD(1), 0x02, 0x08, 0x01, 0x02, 0x00, 0x01, 0x01, 0x02, 0x00, 0x02};
    s_eip712_path path;

    TEST_ASSERT_FALSE(parse(bytes, sizeof(bytes), &path));
}

void test_parse_refuses_a_field_index_that_is_not_a_byte(void) {
    static const uint8_t bytes[] = {VERSION_1, 0x01, 0x02, 0x01, 0x00};
    s_eip712_path path;

    TEST_ASSERT_FALSE(parse(bytes, sizeof(bytes), &path));
}

void test_parse_refuses_an_unknown_tag(void) {
    static const uint8_t bytes[] = {VERSION_1, FIELD(0), 0x04, 0x01, 0x00};
    s_eip712_path path;

    TEST_ASSERT_FALSE(parse(bytes, sizeof(bytes), &path));
}

void test_parse_refuses_too_many_steps(void) {
    uint8_t bytes[3 + (3 * (EIP712_PATH_MAX_SIZE + 1))];
    size_t size = 0;
    s_eip712_path path;

    memcpy(bytes, (const uint8_t[]) {VERSION_1}, 3);
    size = 3;
    for (size_t i = 0; i < EIP712_PATH_MAX_SIZE; ++i) {
        memcpy(&bytes[size], (const uint8_t[]) {FIELD(0)}, 3);
        size += 3;
    }
    TEST_ASSERT_TRUE(parse(bytes, size, &path));
    memcpy(&bytes[size], (const uint8_t[]) {FIELD(0)}, 3);
    size += 3;
    TEST_ASSERT_FALSE(parse(bytes, size, &path));
}

// =============================================================================
// Resolution
// =============================================================================

static void assert_value(const s_parsed_value *value, const uint8_t *expected, size_t length) {
    TEST_ASSERT_EQUAL(length, value->length);
    TEST_ASSERT_EQUAL_MEMORY(expected, value->ptr, length);
}

void test_get_a_leaf_of_the_root(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;
    uint8_t owner[ADDRESS_LENGTH];

    memset(owner, 0xAA, sizeof(owner));
    add_field_step(&path, ORDER_FIELD_OWNER);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(1, collection.size);
    assert_value(&collection.value[0], owner, sizeof(owner));
}

void test_get_a_field_of_every_element(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_ITEMS);
    add_array_step(&path, NO_BOUND, NO_BOUND);
    add_field_step(&path, ITEM_FIELD_PRICE);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(3, collection.size);
    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL(INT256_LENGTH, collection.value[i].length);
        TEST_ASSERT_EQUAL(i + 1, collection.value[i].ptr[INT256_LENGTH - 1]);
    }
}

void test_get_a_range_of_elements(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_ITEMS);
    add_array_step(&path, 1, 3);
    add_field_step(&path, ITEM_FIELD_TOKEN);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(2, collection.size);
    TEST_ASSERT_EQUAL(0x11, collection.value[0].ptr[0]);
    TEST_ASSERT_EQUAL(0x12, collection.value[1].ptr[0]);
}

void test_get_counts_negative_bounds_from_the_end(void) {
    s_eip712_path last = {.version = 1};
    s_eip712_path all_but_last = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&last, ORDER_FIELD_ITEMS);
    add_array_step(&last, -1, NO_BOUND);
    add_field_step(&last, ITEM_FIELD_TOKEN);
    TEST_ASSERT_TRUE(eip712_path_get(&last, &collection));
    TEST_ASSERT_EQUAL(1, collection.size);
    TEST_ASSERT_EQUAL(0x12, collection.value[0].ptr[0]);

    add_field_step(&all_but_last, ORDER_FIELD_ITEMS);
    add_array_step(&all_but_last, NO_BOUND, -1);
    add_field_step(&all_but_last, ITEM_FIELD_TOKEN);
    TEST_ASSERT_TRUE(eip712_path_get(&all_but_last, &collection));
    TEST_ASSERT_EQUAL(2, collection.size);
    TEST_ASSERT_EQUAL(0x10, collection.value[0].ptr[0]);
    TEST_ASSERT_EQUAL(0x11, collection.value[1].ptr[0]);
}

void test_get_every_value_of_every_dimension(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_GRID);
    add_array_step(&path, NO_BOUND, NO_BOUND);
    add_array_step(&path, NO_BOUND, NO_BOUND);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(4, collection.size);
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL(1, collection.value[i].length);
        TEST_ASSERT_EQUAL(i + 1, collection.value[i].ptr[0]);
    }
}

void test_get_a_slice_of_each_dimension(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_GRID);
    add_array_step(&path, 1, 2);
    add_array_step(&path, 0, 1);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(1, collection.size);
    TEST_ASSERT_EQUAL(3, collection.value[0].ptr[0]);
}

void test_get_a_slice_of_the_leaf(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_BLOB);
    add_leaf_step(&path, 1, 3);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(1, collection.size);
    assert_value(&collection.value[0], &k_blob[1], 2);
    TEST_ASSERT_EQUAL(1, collection.value[0].offset);
    TEST_ASSERT_EQUAL(sizeof(k_blob), collection.value[0].size);
}

void test_get_a_slice_of_the_leaf_counts_negative_bounds_from_the_end(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_BLOB);
    add_leaf_step(&path, -2, NO_BOUND);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    assert_value(&collection.value[0], &k_blob[sizeof(k_blob) - 2], 2);
}

// the end is exclusive: -1 leaves the last byte out
void test_get_a_slice_of_the_leaf_counts_a_negative_end_from_the_end(void) {
    s_eip712_path path = {.version = 1};
    s_eip712_path too_far = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_BLOB);
    add_leaf_step(&path, 1, -1);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    assert_value(&collection.value[0], &k_blob[1], sizeof(k_blob) - 2);

    // an end before the start of the value selects nothing, which is refused for a leaf
    add_field_step(&too_far, ORDER_FIELD_BLOB);
    add_leaf_step(&too_far, NO_BOUND, -7);
    TEST_ASSERT_FALSE(eip712_path_get(&too_far, &collection));
}

void test_get_a_string(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_NAME);
    add_leaf_step(&path, 1, NO_BOUND);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    assert_value(&collection.value[0], (const uint8_t *) "ob", 2);
}

// An array slice that selects nothing is valid, it leads to no value
void test_get_nothing_from_an_empty_array(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection = {.size = 9};

    td_deinit();
    build_schema();
    build_message(0);
    add_field_step(&path, ORDER_FIELD_ITEMS);
    add_array_step(&path, NO_BOUND, NO_BOUND);
    add_field_step(&path, ITEM_FIELD_PRICE);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(0, collection.size);
}

void test_get_nothing_from_a_selection_that_is_empty(void) {
    s_eip712_path same_bounds = {.version = 1};
    s_eip712_path inverted = {.version = 1};
    s_eip712_path past_the_start = {.version = 1};
    s_parsed_value_collection collection = {.size = 9};

    add_field_step(&same_bounds, ORDER_FIELD_ITEMS);
    add_array_step(&same_bounds, 2, 2);
    add_field_step(&same_bounds, ITEM_FIELD_PRICE);
    TEST_ASSERT_TRUE(eip712_path_get(&same_bounds, &collection));
    TEST_ASSERT_EQUAL(0, collection.size);

    add_field_step(&inverted, ORDER_FIELD_ITEMS);
    add_array_step(&inverted, 2, 1);
    add_field_step(&inverted, ITEM_FIELD_PRICE);
    TEST_ASSERT_TRUE(eip712_path_get(&inverted, &collection));
    TEST_ASSERT_EQUAL(0, collection.size);

    add_field_step(&past_the_start, ORDER_FIELD_ITEMS);
    add_array_step(&past_the_start, 5, NO_BOUND);
    add_field_step(&past_the_start, ITEM_FIELD_PRICE);
    TEST_ASSERT_TRUE(eip712_path_get(&past_the_start, &collection));
    TEST_ASSERT_EQUAL(0, collection.size);
}

// An empty selection in an inner dimension does not stop the walk of the outer one
void test_get_skips_an_empty_inner_selection(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_GRID);
    add_array_step(&path, NO_BOUND, NO_BOUND);
    add_array_step(&path, 1, 1);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(0, collection.size);
}

void test_get_refuses_bounds_outside_of_the_array(void) {
    s_eip712_path start_before = {.version = 1};
    s_eip712_path end_before = {.version = 1};
    s_eip712_path end_past = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&start_before, ORDER_FIELD_ITEMS);
    add_array_step(&start_before, -4, NO_BOUND);
    TEST_ASSERT_FALSE(eip712_path_get(&start_before, &collection));

    add_field_step(&end_before, ORDER_FIELD_ITEMS);
    add_array_step(&end_before, NO_BOUND, -4);
    TEST_ASSERT_FALSE(eip712_path_get(&end_before, &collection));

    add_field_step(&end_past, ORDER_FIELD_ITEMS);
    add_array_step(&end_past, NO_BOUND, 4);
    TEST_ASSERT_FALSE(eip712_path_get(&end_past, &collection));
}

void test_get_refuses_a_slice_of_the_leaf_outside_of_the_value(void) {
    s_eip712_path empty = {.version = 1};
    s_eip712_path past = {.version = 1};
    s_eip712_path before = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&empty, ORDER_FIELD_BLOB);
    add_leaf_step(&empty, 2, 2);
    TEST_ASSERT_FALSE(eip712_path_get(&empty, &collection));

    add_field_step(&past, ORDER_FIELD_BLOB);
    add_leaf_step(&past, NO_BOUND, 7);
    TEST_ASSERT_FALSE(eip712_path_get(&past, &collection));

    add_field_step(&before, ORDER_FIELD_BLOB);
    add_leaf_step(&before, -7, NO_BOUND);
    TEST_ASSERT_FALSE(eip712_path_get(&before, &collection));
}

void test_get_refuses_a_slice_of_the_leaf_of_a_number(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_OWNER);
    add_leaf_step(&path, 0, 1);
    TEST_ASSERT_FALSE(eip712_path_get(&path, &collection));
}

void test_get_refuses_a_field_that_does_not_exist(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, 5);
    TEST_ASSERT_FALSE(eip712_path_get(&path, &collection));
}

void test_get_refuses_a_step_that_does_not_fit_the_node(void) {
    s_eip712_path field_of_array = {.version = 1};
    s_eip712_path slice_of_struct = {.version = 1};
    s_eip712_path slice_of_leaf = {.version = 1};
    s_eip712_path field_of_leaf = {.version = 1};
    s_parsed_value_collection collection;

    // a field of the items array, without choosing an element
    add_field_step(&field_of_array, ORDER_FIELD_ITEMS);
    add_field_step(&field_of_array, ITEM_FIELD_PRICE);
    TEST_ASSERT_FALSE(eip712_path_get(&field_of_array, &collection));

    // the root is a struct
    slice_of_struct.elements[0].type = EIP712_ELEMENT_ARRAY_SLICE;
    slice_of_struct.size = 1;
    TEST_ASSERT_FALSE(eip712_path_get(&slice_of_struct, &collection));

    add_field_step(&slice_of_leaf, ORDER_FIELD_OWNER);
    add_array_step(&slice_of_leaf, NO_BOUND, NO_BOUND);
    TEST_ASSERT_FALSE(eip712_path_get(&slice_of_leaf, &collection));

    add_field_step(&field_of_leaf, ORDER_FIELD_OWNER);
    add_field_step(&field_of_leaf, 0);
    TEST_ASSERT_FALSE(eip712_path_get(&field_of_leaf, &collection));
}

void test_get_refuses_a_path_that_does_not_end_on_a_leaf(void) {
    s_eip712_path on_array = {.version = 1};
    s_eip712_path on_struct = {.version = 1};
    s_eip712_path on_partial_dimensions = {.version = 1};
    s_eip712_path on_root = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&on_array, ORDER_FIELD_ITEMS);
    TEST_ASSERT_FALSE(eip712_path_get(&on_array, &collection));

    add_field_step(&on_struct, ORDER_FIELD_ITEMS);
    add_array_step(&on_struct, NO_BOUND, NO_BOUND);
    TEST_ASSERT_FALSE(eip712_path_get(&on_struct, &collection));

    // the grid has two dimensions
    add_field_step(&on_partial_dimensions, ORDER_FIELD_GRID);
    add_array_step(&on_partial_dimensions, NO_BOUND, NO_BOUND);
    TEST_ASSERT_FALSE(eip712_path_get(&on_partial_dimensions, &collection));

    TEST_ASSERT_FALSE(eip712_path_get(&on_root, &collection));
}

void test_get_refuses_more_values_than_a_collection_holds(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    add_field_step(&path, ORDER_FIELD_ITEMS);
    add_array_step(&path, NO_BOUND, NO_BOUND);
    add_field_step(&path, ITEM_FIELD_PRICE);

    td_deinit();
    build_schema();
    build_message(MAX_VALUE_COLLECTION_SIZE);
    TEST_ASSERT_TRUE(eip712_path_get(&path, &collection));
    TEST_ASSERT_EQUAL(MAX_VALUE_COLLECTION_SIZE, collection.size);

    td_deinit();
    build_schema();
    build_message(MAX_VALUE_COLLECTION_SIZE + 1);
    TEST_ASSERT_FALSE(eip712_path_get(&path, &collection));
}

void test_get_refuses_a_path_without_message(void) {
    s_eip712_path path = {.version = 1};
    s_parsed_value_collection collection;

    td_deinit();
    add_field_step(&path, ORDER_FIELD_OWNER);
    TEST_ASSERT_FALSE(eip712_path_get(&path, &collection));
}

// =============================================================================
// Runner
// =============================================================================

void setUp(void) {
    td_deinit();
    build_schema();
    build_message(3);
}

void tearDown(void) {
    td_deinit();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_field_steps);
    RUN_TEST(test_parse_keeps_the_order_of_steps);
    RUN_TEST(test_parse_refuses_a_missing_or_unsupported_version);
    RUN_TEST(test_parse_refuses_a_path_that_does_not_start_with_a_field);
    RUN_TEST(test_parse_refuses_steps_below_the_leaf_slice);
    RUN_TEST(test_parse_reads_one_byte_bounds_as_not_negative);
    RUN_TEST(test_parse_reads_one_byte_and_two_byte_bounds_alike);
    RUN_TEST(test_parse_refuses_bounds_of_another_length);
    RUN_TEST(test_parse_refuses_a_repeated_bound);
    RUN_TEST(test_parse_refuses_a_field_index_that_is_not_a_byte);
    RUN_TEST(test_parse_refuses_an_unknown_tag);
    RUN_TEST(test_parse_refuses_too_many_steps);
    RUN_TEST(test_get_a_leaf_of_the_root);
    RUN_TEST(test_get_a_field_of_every_element);
    RUN_TEST(test_get_a_range_of_elements);
    RUN_TEST(test_get_counts_negative_bounds_from_the_end);
    RUN_TEST(test_get_every_value_of_every_dimension);
    RUN_TEST(test_get_a_slice_of_each_dimension);
    RUN_TEST(test_get_a_slice_of_the_leaf);
    RUN_TEST(test_get_a_slice_of_the_leaf_counts_negative_bounds_from_the_end);
    RUN_TEST(test_get_a_slice_of_the_leaf_counts_a_negative_end_from_the_end);
    RUN_TEST(test_get_a_string);
    RUN_TEST(test_get_nothing_from_an_empty_array);
    RUN_TEST(test_get_nothing_from_a_selection_that_is_empty);
    RUN_TEST(test_get_skips_an_empty_inner_selection);
    RUN_TEST(test_get_refuses_bounds_outside_of_the_array);
    RUN_TEST(test_get_refuses_a_slice_of_the_leaf_outside_of_the_value);
    RUN_TEST(test_get_refuses_a_slice_of_the_leaf_of_a_number);
    RUN_TEST(test_get_refuses_a_field_that_does_not_exist);
    RUN_TEST(test_get_refuses_a_step_that_does_not_fit_the_node);
    RUN_TEST(test_get_refuses_a_path_that_does_not_end_on_a_leaf);
    RUN_TEST(test_get_refuses_more_values_than_a_collection_holds);
    RUN_TEST(test_get_refuses_a_path_without_message);
    return UNITY_END();
}
