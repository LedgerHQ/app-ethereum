#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "common_utils.h"  // ADDRESS_LENGTH
#include "operation_info.h"

/**
 * What a piece of signed metadata (an enum value, a map entry) is attached to: what identifies the
 * operation it is for, as the OPERATION_INFO of the operation does
 *
 * The context tells which of the two kinds of operation it is, which the tags of the metadata
 * give: the metadata can be received before the operation it is for.
 */
typedef struct {
    e_operation_info_context context;
    union {
        struct {  // SIGN_TX: a function of a contract
            uint64_t chain_id;
            uint8_t contract_addr[ADDRESS_LENGTH];
            uint8_t selector[CALLDATA_SELECTOR_SIZE];
        };
        struct {  // EIP712: a primary type of a domain
            uint8_t domain_separator[INT256_LENGTH];
            uint8_t primary_type_hash[INT256_LENGTH];
        };
    };
} s_operation_key;

/**
 * Check whether metadata attached to a key applies to an operation
 *
 * A call reaches a function of its callee, or of the implementation behind it when the callee is a
 * proxy: the metadata is attached to the implementation. A typed message names no proxy.
 *
 * @param[in] attached the key the metadata is attached to
 * @param[in] operation the key of the operation
 * @return whether the metadata applies to the operation
 */
bool operation_key_matches(const s_operation_key *attached, const s_operation_key *operation);
