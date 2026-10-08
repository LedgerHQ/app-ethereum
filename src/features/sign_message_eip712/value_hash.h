#pragma once

#include <stdbool.h>
#include "typed_data.h"

/**
 * Run the hash pass over the complete EIP-712 value tree.
 *
 * Computes hashStruct(domain) and hashStruct(message) via recursive traversal
 * and writes results to tmpCtx.messageSigningContext712.domainHash and .messageHash.
 *
 * Must be called only after v1_is_complete() returns true.
 *
 * @return whether the hash pass was successful
 */
bool value_hash_pass(const s_eip712_impl *impl);

/**
 * Compute hashStruct(domain) over the domain value tree, once it is complete
 *
 * @param[in] impl the value trees
 * @param[out] out the hash of the domain
 * @return whether the hash was computed
 */
bool value_hash_domain(const s_eip712_impl *impl, uint8_t out[CX_KECCAK_256_SIZE]);
