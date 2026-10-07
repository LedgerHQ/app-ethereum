#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lists.h"
#include "common_utils.h"
#include "plugin_utils.h"
#include "tlv_library.h"
#include "cx.h"

#define MAP_ENTRY_MAX_KEY_SIZE   32
#define MAP_ENTRY_MAX_VALUE_SIZE 32

typedef struct {
    flist_node_t _list;
    uint64_t chain_id;
    uint8_t contract_addr[ADDRESS_LENGTH];
    uint8_t selector[SELECTOR_SIZE];
    uint8_t id;
    uint8_t key[MAP_ENTRY_MAX_KEY_SIZE];
    uint8_t key_size;
    uint8_t value[MAP_ENTRY_MAX_VALUE_SIZE];
    uint8_t value_size;
} s_map_entry;

typedef struct {
    s_map_entry entry;
    uint8_t sig_size;
    const uint8_t *sig;
    cx_sha256_t hash_ctx;
    TLV_reception_t received_tags;
} s_map_entry_ctx;

bool handle_map_entry_tlv_payload(const buffer_t *buf, s_map_entry_ctx *context);
bool verify_map_entry_struct(const s_map_entry_ctx *context);
const s_map_entry *get_matching_map_entry(uint8_t id, const uint8_t *key, uint8_t key_size);
void map_entry_cleanup(void);
