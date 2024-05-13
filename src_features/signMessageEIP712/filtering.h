#ifndef FILTERING_H_
#define FILTERING_H_

#ifdef HAVE_EIP712_FULL_SUPPORT

#include <stdbool.h>
#include <stdint.h>

bool filtering_message_info(const uint8_t *payload, uint8_t length);
bool filtering_date_time(const uint8_t *payload, uint8_t length);
bool filtering_amount_join_token(const uint8_t *payload, uint8_t length);
bool filtering_amount_join_value(const uint8_t *payload, uint8_t length);
bool filtering_raw_field(const uint8_t *payload, uint8_t length);

#endif  // HAVE_EIP712_FULL_SUPPORT

#endif  // FILTERING_H_
