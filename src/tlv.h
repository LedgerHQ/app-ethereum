#ifndef TLV_H_
#define TLV_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t tag;
    uint8_t length;
    const uint8_t *value;
} s_tlv_data;

typedef bool(f_tlv_handler)(const s_tlv_data *data, void *param);

typedef struct {
    uint8_t tag;
    f_tlv_handler *func;
    bool required;
} s_tlv_handler;

typedef struct {
    uint8_t tag;
    cx_hash_t *ctx;
} s_tlv_sig;

bool parse_tlv(const uint8_t *payload,
               size_t payload_size,
               const s_tlv_handler *handlers,
               uint8_t handler_count,
               void *param,
               s_tlv_sig *sig);

#endif  // TLV_H_
