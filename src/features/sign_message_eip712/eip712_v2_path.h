#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "buffer.h"
#include "gcs_parsed_value.h"
#include "gtp_path_slice.h"  // s_slice_args

// The deepest value tree has 16 levels, each one being a struct field or an array dimension, and
// the path may end with a slice of the leaf
#define EIP712_PATH_MAX_SIZE 17

typedef enum {
    EIP712_ELEMENT_STRUCT_FIELD,  // descend into a field of a struct
    EIP712_ELEMENT_ARRAY_SLICE,   // select elements of an array dimension
    EIP712_ELEMENT_LEAF_SLICE,    // select bytes of the leaf
} e_eip712_path_element_type;

typedef struct {
    e_eip712_path_element_type type;
    union {
        uint8_t field_index;  // EIP712_ELEMENT_STRUCT_FIELD
        s_slice_args slice;   // EIP712_ELEMENT_ARRAY_SLICE, EIP712_ELEMENT_LEAF_SLICE
    };
} s_eip712_path_element;

// A location in the message's value tree, as the steps from its root down to a leaf
typedef struct {
    uint8_t version;
    uint8_t size;
    s_eip712_path_element elements[EIP712_PATH_MAX_SIZE];
} s_eip712_path;

typedef struct {
    s_eip712_path *path;
} s_eip712_path_context;

/**
 * Parse and check an EIP712_PATH struct
 *
 * @param[in] buf the struct payload
 * @param[out] context the path to fill
 * @return whether the path is well formed
 */
bool handle_eip712_path_struct(const buffer_t *buf, s_eip712_path_context *context);

/**
 * Get the values a path leads to in the message's value tree
 *
 * An array slice that selects no element is valid and leads to no value. The values point into
 * the tree, which has to outlive them, and own nothing.
 *
 * @param[in] path the path
 * @param[out] collection the leaves it leads to
 * @return whether the path fits the message, and leads to at most MAX_VALUE_COLLECTION_SIZE values
 */
bool eip712_path_get(const s_eip712_path *path, s_parsed_value_collection *collection);
