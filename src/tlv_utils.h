#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "os_print.h"
#include "tlv_library.h"

// Macro to check the field value
#define CHECK_FIELD_VALUE(tag, value, expected)  \
    do {                                         \
        if (value != expected) {                 \
            PRINTF("%s Value mismatch!\n", tag); \
            return false;                        \
        }                                        \
    } while (0)

// Helper functions for common TLV parsing operations
bool tlv_check_struct_type(const tlv_data_t *data, uint8_t expected);
bool tlv_check_struct_version(const tlv_data_t *data, uint8_t expected);
bool tlv_check_challenge(const tlv_data_t *data);
bool tlv_get_chain_id(const tlv_data_t *data, uint64_t *chain_id);
bool tlv_get_hash(const tlv_data_t *data, uint8_t *out, uint16_t max_size);
bool tlv_get_address(const tlv_data_t *data, uint8_t *out);
bool tlv_get_printable_string(const tlv_data_t *data,
                              char *out,
                              uint32_t min_len,
                              uint32_t max_len);
bool tlv_get_uint16_range(const tlv_data_t *data,
                          uint16_t *out,
                          uint16_t min_val,
                          uint16_t max_val);
bool tlv_get_uint8_range(const tlv_data_t *data, uint8_t *out, uint8_t min_val, uint8_t max_val);
bool tlv_check_uint8(const tlv_data_t *data, uint8_t expected);
