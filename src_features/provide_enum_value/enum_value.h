#ifndef ENUM_VALUE_H_
#define ENUM_VALUE_H_

#include <stdbool.h>
#include "common_utils.h"  // ADDRESS_LENGTH
#include "plugin_utils.h"  // SELECTOR_SIZE
#include "tlv.h"
#include "cx.h"

typedef struct {
    uint64_t chain_id;
    uint8_t contract_addr[ADDRESS_LENGTH];
    uint8_t selector[SELECTOR_SIZE];
    uint8_t id;
    uint8_t value;
    char name[21];
} s_enum_value_entry;

typedef struct {
    uint8_t version;
    s_enum_value_entry entry;
    uint8_t signature_length;
    uint8_t signature[73];
} s_enum_value;

typedef struct {
    s_enum_value enum_value;
    cx_sha256_t struct_hash;
} s_enum_value_ctx;

bool handle_enum_value_struct(const s_tlv_data *data, s_enum_value_ctx *context);
bool verify_enum_value_struct(const s_enum_value_ctx *context);
const char *get_matching_enum_name(const uint64_t *chain_id,
                                   const uint8_t *contract_addr,
                                   const uint8_t *selector,
                                   uint8_t id,
                                   uint8_t value);

#endif  // !ENUM_VALUE_H_
