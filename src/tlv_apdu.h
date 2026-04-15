#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "buffer.h"

typedef bool (*f_tlv_payload_handler)(const buffer_t *payload);

bool tlv_from_apdu(bool first_chunk,
                   uint8_t lc,
                   const uint8_t *payload,
                   f_tlv_payload_handler handler);
