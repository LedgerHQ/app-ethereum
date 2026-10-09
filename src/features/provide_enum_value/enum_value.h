#pragma once

#include <stdbool.h>
#include "lists.h"
#include "common_utils.h"  // ADDRESS_LENGTH
#include "operation_key.h"
#include "tlv_library.h"
#include "cx.h"

#define MAX_ENUM_NAME_SIZE 21

typedef struct {
    flist_node_t _list;
    s_operation_key operation_key;
    uint8_t id;
    uint8_t value;
    char name[MAX_ENUM_NAME_SIZE];
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

/**
 * Get the enum value stored for an operation
 *
 * @param[in] operation_key the key of the operation, see \ref operation_key_matches
 * @param[in] id the identifier of the enum
 * @param[in] value the value in the enum
 * @return the stored entry, NULL if there is none
 */
const s_enum_value_entry *get_matching_enum(const s_operation_key *operation_key,
                                            uint8_t id,
                                            uint8_t value);
void enum_value_cleanup(void);
