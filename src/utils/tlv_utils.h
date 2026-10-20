#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "os_print.h"
#include "tlv_library.h"

// Helper functions for common TLV parsing operations
bool tlv_check_challenge(const tlv_data_t *data);
bool tlv_get_chain_id(const tlv_data_t *data, uint64_t *chain_id);
bool tlv_get_hash(const tlv_data_t *data, uint8_t *out, uint16_t size);
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

/**
 * @brief Read an int16_t sent on one or two bytes, big-endian.
 *
 * The length gives the sign: one byte is 0 to 255, and two bytes are two's complement, so 0xFF is
 * 255 and 0xFFFF is -1. Any other length is refused.
 *
 * @param[in] data TLV data to read from
 * @param[out] out the value read
 * @return whether the data was a valid int16_t
 */
static inline bool tlv_get_int16(const tlv_data_t *data, int16_t *out) {
    uint16_t value;

    if ((data == NULL) || (out == NULL) || (data->value.ptr == NULL) ||
        (data->value.size > sizeof(value)) || !get_uint16_t_from_tlv_data(data, &value)) {
        return false;
    }
    *out = (int16_t) value;
    return true;
}
