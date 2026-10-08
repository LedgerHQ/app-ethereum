#pragma once

#include <stdbool.h>
#include "buffer.h"

/**
 * Handle a complete EIP712_MESSAGE_INFO payload
 *
 * The struct is checked like any TRANSACTION_INFO, then bound to the message it describes, and
 * made the root of the operations (see \ref operation_ctx_init_message), which keeps it for the
 * length of the signing flow.
 *
 * @param[in] buf the complete payload
 * @return whether the struct was accepted
 */
bool handle_eip712_v2_message_info_struct(const buffer_t *buf);
