#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "apdu_constants.h"
#include "fuzz_defs.h"

#define INS_FUZZ_SWAP_CHECK_ADDRESS    0xF0
#define INS_FUZZ_SWAP_PRINTABLE_AMOUNT 0xF1
#define INS_FUZZ_SWAP_COPY_TRANSACTION 0xF2
#define INS_FUZZ_PLUGIN_UTILS          0xF3

typedef enum {
    FUZZ_COMMAND_GROUP_APDU = 0,
    FUZZ_COMMAND_GROUP_EXTENSION,
} fuzz_command_group_t;

typedef enum {
    FUZZ_PAYLOAD_RAW = 0,
    FUZZ_PAYLOAD_TLV,
} fuzz_payload_kind_t;

typedef enum {
    FUZZ_TLV_NONE = 0,
    FUZZ_TLV_TRUSTED_NAME,
    FUZZ_TLV_ENUM_VALUE,
    FUZZ_TLV_GATING,
    FUZZ_TLV_TX_SIMULATION,
    FUZZ_TLV_PROXY_INFO,
    FUZZ_TLV_NETWORK_INFO,
    FUZZ_TLV_SAFE_DESCRIPTOR,
    FUZZ_TLV_GTP_TX_INFO,
    FUZZ_TLV_GTP_FIELD,
    FUZZ_TLV_AUTH_7702,
    FUZZ_TLV_MAP_ENTRY,
} fuzz_tlv_kind_t;

typedef struct {
    fuzz_command_group_t group;
    const char *name;
    uint8_t ins;
    fuzz_payload_kind_t payload_kind;
    fuzz_tlv_kind_t tlv_kind;
    bool allow_empty_data;
} fuzz_command_meta_t;

extern const fuzz_command_spec_t fuzz_commands[];
extern const fuzz_command_meta_t fuzz_command_metas[];
extern const size_t fuzz_n_commands;

const fuzz_command_meta_t *fuzz_command_meta_by_ins(uint8_t ins);
bool fuzz_command_is_extension_ins(uint8_t ins);
