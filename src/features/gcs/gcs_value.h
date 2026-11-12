#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"
#include "gtp_data_path.h"
#include "eip712_v2_path.h"
#include "calldata.h"

typedef enum {
    TF_UINT = 1,
    TF_INT,
    TF_UFIXED,
    TF_FIXED,
    TF_ADDRESS,
    TF_BOOL,
    TF_BYTES,
    TF_STRING,
} e_type_family;

typedef enum {
    CP_FROM = 0,
    CP_TO,
    CP_VALUE,
    CP_CHAIN_ID,
} e_container_path;

typedef enum {
    SOURCE_CALLDATA,
    SOURCE_RLP,
    SOURCE_CONSTANT,
    SOURCE_MAP_REF,
    SOURCE_EIP712,
} e_value_source;

typedef struct {
    uint8_t size;
    uint8_t buf[CALLDATA_CHUNK_SIZE];
} s_constant;

// Raw TLV bytes of the KEY VALUE struct (avoids recursive s_value embedding)
#define MAP_REF_KEY_TLV_MAX_SIZE 48

typedef struct {
    uint8_t id;
    uint8_t key_tlv[MAP_REF_KEY_TLV_MAX_SIZE];
    uint8_t key_tlv_size;
} s_map_ref;

typedef struct {
    uint8_t version;
    e_type_family type_family;
    uint8_t type_size;
    union {
        s_data_path data_path;
        s_eip712_path eip712_path;
        e_container_path container_path;
        s_constant constant;
        s_map_ref map_ref;
    };
    e_value_source source;
} s_value;

typedef struct {
    s_value *value;
} s_value_context;

bool handle_value_struct(const buffer_t *buf, s_value_context *context);
bool value_get(const s_value *value, s_parsed_value_collection *collection);
void value_cleanup(const s_value *value, const s_parsed_value_collection *collection);

/**
 * Identify where a value is read: the same key for the same source, path or constant
 *
 * @param[in] value the value
 * @return the key
 */
uint32_t value_source_key(const s_value *value);

/**
 * Record that a row of the review shows a value, read for the operation being described
 *
 * To be called once the row is in the review, so that the review does not add its own row for it.
 *
 * @param[in] value the value the row shows
 * @param[in] index position of the value in the collection it was read in
 */
void value_set_shown(const s_value *value, uint8_t index);
