#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "buffer.h"
#include "common_utils.h"  // CX_KECCAK_256_SIZE

/**
 * Check a domain separator is the one of the message being signed
 *
 * It is the hash of the domain of the message. When the verifyingContract of the domain is the
 * proxy of a received PROXY_INFO for the chain of the domain, and that PROXY_INFO has no selector,
 * it can also be the hash of the domain with the implementation in place of that contract.
 *
 * @param[in] separator the domain separator
 * @return whether it is the one of the message
 */
bool eip712_v2_is_domain_separator(const uint8_t separator[CX_KECCAK_256_SIZE]);

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
