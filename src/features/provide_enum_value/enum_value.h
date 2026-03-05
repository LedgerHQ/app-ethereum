#pragma once

#include <stdbool.h>
#include "lists.h"
#include "common_utils.h"  // ADDRESS_LENGTH
#include "plugin_utils.h"  // SELECTOR_SIZE
#include "tlv_library.h"
#include "cx.h"
#include "signature.h"

typedef struct {
    flist_node_t _list;
    uint64_t chain_id;
    uint8_t contract_addr[ADDRESS_LENGTH];
    uint8_t selector[SELECTOR_SIZE];
    uint8_t id;
    uint8_t value;
    char name[21];
} s_enum_value_entry;

typedef struct {
    s_enum_value_entry entry;
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
    TLV_reception_t received_tags;
} s_enum_value_ctx;

bool handle_enum_value_tlv_payload(const buffer_t *buf, s_enum_value_ctx *context);
bool verify_enum_value_struct(const s_enum_value_ctx *context);
const s_enum_value_entry *get_matching_enum(const uint64_t *chain_id,
                                            const uint8_t *contract_addr,
                                            const uint8_t *selector,
                                            uint8_t id,
                                            uint8_t value);
void enum_value_cleanup(void);
