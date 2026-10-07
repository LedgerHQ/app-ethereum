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
