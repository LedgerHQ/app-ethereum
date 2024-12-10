#ifndef GTP_VALUE_H_
#define GTP_VALUE_H_

#include <stdint.h>
#include <stdbool.h>
#include "tlv.h"
#include "gtp_data_path.h"

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
} e_container_path;

typedef enum {
    SOURCE_CALLDATA,
    SOURCE_RLP,
    SOURCE_CONSTANT,
} e_value_source;

typedef struct {
    uint8_t version;
    e_type_family type_family;
    uint8_t type_size;
    s_data_path data_path;
    e_container_path container_path;
    e_value_source source;
} s_value;

typedef struct {
    s_value *value;
} s_value_context;

bool handle_value_struct(const s_tlv_data *data, s_value_context *context);
bool value_get(const s_value *value, s_parsed_value_collection *collection);

#endif // !GTP_VALUE_H_
