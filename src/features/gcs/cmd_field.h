#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "buffer.h"

/**
 * Handle one complete FIELD descriptor of the operation being described
 *
 * The descriptor is parsed and checked, added to the running hash that attests the descriptors of
 * the operation, and formatted into the field table. A descriptor refused after it was hashed
 * leaves no trace in the hash.
 *
 * @param[in] buf the complete descriptor
 * @return whether the descriptor was accepted
 */
bool handle_field_tlv_payload(const buffer_t *buf);
