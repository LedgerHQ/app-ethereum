#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "buffer.h"

typedef bool (*f_tlv_payload_handler)(const buffer_t *payload);

typedef enum {
    TLV_APDU_ERROR = 0,
    TLV_APDU_PENDING,
    TLV_APDU_SUCCESS,
} e_tlv_apdu_ret;

e_tlv_apdu_ret tlv_from_apdu(bool first_chunk,
                             uint8_t lc,
                             const uint8_t *payload,
                             f_tlv_payload_handler handler);
