#ifndef TLV_APDU_H_
#define TLV_APDU_H_

#include <stdbool.h>
#include <stdint.h>

typedef bool (*f_tlv_payload_handler)(const uint8_t *payload, uint16_t size, bool to_free);

bool tlv_from_apdu(bool first_chunk,
                   uint8_t lc,
                   const uint8_t *payload,
                   f_tlv_payload_handler handler);

#endif  // !TLV_APDU_H_
