#ifndef TLV_H_
#define TLV_H_

#include <stdint.h>
#include <stdbool.h>

#define TLV_TAG_ERROR_MSG "Error: Unknown TLV tag (0x%02x)\n"

typedef struct {
    uint8_t tag;           // T
    uint16_t length;       // L
    const uint8_t *value;  // V

    // in case the handler needs to do some processing on the raw TLV-encoded data (like computing a
    // signature)
    const uint8_t *raw;
    uint16_t raw_size;
} s_tlv_data;

typedef bool (*f_tlv_data_handler)(const s_tlv_data *, void *);

bool tlv_parse(const uint8_t *payload, uint16_t size, f_tlv_data_handler handler, void *context);

#endif  // !TLV_H_
