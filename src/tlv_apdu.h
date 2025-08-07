#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef bool (*f_tlv_payload_handler)(const uint8_t *payload, uint16_t size);

bool tlv_from_apdu(bool first_chunk,
                   uint8_t lc,
                   const uint8_t *payload,
                   f_tlv_payload_handler handler);
