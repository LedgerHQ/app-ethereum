#ifndef GTP_DATA_PATH_H_
#define GTP_DATA_PATH_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "tlv.h"
#include "gtp_parsed_value.h"
#include "gtp_path_array.h"
#include "gtp_path_slice.h"

#define PATH_MAX_SIZE 16

typedef enum {
    ELEMENT_TYPE_TUPLE,
    ELEMENT_TYPE_ARRAY,
    ELEMENT_TYPE_REF,
    ELEMENT_TYPE_LEAF,
    ELEMENT_TYPE_SLICE,
} e_path_element_type;

typedef enum {
    LEAF_TYPE_ARRAY = 0x01,
    LEAF_TYPE_TUPLE = 0x02,
    LEAF_TYPE_STATIC = 0x03,
    LEAF_TYPE_DYNAMIC = 0x04,
} e_path_leaf_type;

typedef struct {
    uint16_t value;
} s_tuple_args;

typedef struct {
    e_path_leaf_type type;
} s_leaf_args;

typedef struct {
    e_path_element_type type;
    union {
        s_tuple_args tuple;
        s_array_args array;
        s_leaf_args leaf;
        s_slice_args slice;
    };
} s_path_element;

typedef struct {
    uint8_t version;
    uint8_t size;
    s_path_element elements[PATH_MAX_SIZE];
} s_data_path;

typedef struct {
    s_data_path *data_path;
} s_data_path_context;

bool handle_data_path_struct(const s_tlv_data *data, s_data_path_context *context);
bool data_path_get(const s_data_path *data_path, s_parsed_value_collection *collection);

#endif  // GTP_DATA_PATH_H_
