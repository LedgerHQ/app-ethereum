#ifndef TLV_H_
#define TLV_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t tag;
    size_t length;
    const uint8_t *value;
} s_tlv_data;

// forward declaration
typedef struct tlv_handler_param s_tlv_handler_param;

typedef bool(f_tlv_handler)(const s_tlv_data *data, s_tlv_handler_param *param);

size_t der_encode_value(uint8_t *payload, size_t payload_size, size_t value);

bool parse_tlv(const uint8_t *payload,
               size_t payload_size,
               f_tlv_handler *handler,
               s_tlv_handler_param *param);

#endif  // TLV_H_
