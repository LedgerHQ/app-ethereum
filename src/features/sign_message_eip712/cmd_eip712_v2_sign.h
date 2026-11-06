#pragma once

#include <stdint.h>

/**
 * Process the EIP-712 V2 sign command
 *
 * The signing derivation path is not carried here: it was delivered alongside the value tree,
 * in EIP712_VALUES. This command only triggers the signature of the already-received message.
 *
 * @param[in] p1 command's P1 byte, must be 0
 * @param[in] p2 command's P2 byte
 * @param[in] lc command's data length
 * @return status word
 */
uint16_t handle_eip712_v2_sign(uint8_t p1, uint8_t p2, uint8_t lc);
