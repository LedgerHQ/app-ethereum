#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cx.h"
#include "lists.h"
#include "common_utils.h"  // ADDRESS_LENGTH, INT256_LENGTH
#include "operation_info.h"
#include "operation_key.h"
#include "calldata.h"
#include "gcs_field.h"

/**
 * An operation the FIELD descriptors are evaluated against: a call, or the typed message that makes
 * calls.
 *
 * The calls form a list in execution order. A typed message is not one of them: it has no
 * transaction of its own to be the root of the calls it makes, so it is kept apart, and is their
 * root. The root of the operations is the typed message if there is one, else the first call, which
 * is the transaction being signed.
 */
typedef struct {
    list_node_t _list;       // for a call only
    s_operation_info *info;  // TRANSACTION_INFO of a call, EIP712_MESSAGE_INFO of a message
    s_calldata *calldata;
    uint8_t from[ADDRESS_LENGTH];  // for a message, the account that signs
    uint8_t to[ADDRESS_LENGTH];    // for a message, the verifyingContract of its domain
    uint8_t amount[INT256_LENGTH];
    uint64_t chain_id;  // only meaningful with has_chain_id

    cx_sha3_t fields_hash_ctx;
    bool has_amount : 1;
    bool has_to : 1;  // always set for a call
    bool has_chain_id
        : 1;  // unset for a message whose domain has no chain, and the calls taking it
} s_operation_ctx;

extern s_calldata *g_parked_calldata;

bool operation_ctx_is_root(void);
/**
 * @return whether a call is being described, that is whether a description was matched to it and
 *         it is not complete yet
 */
bool operation_ctx_is_describing_call(void);
/**
 * @return the number of calls, which a typed message is not
 */
size_t get_operation_ctx_count(void);
const s_operation_info *get_root_operation_info(void);
s_calldata *get_current_calldata(void);
s_calldata *get_root_calldata(void);

// What the descriptors are evaluated against: the operation being described, which is the call
// being described or else the typed message.

/**
 * @return the info of the operation being described, NULL if there is none yet
 */
const s_operation_info *get_current_operation_info(void);
/**
 * @param[out] chain_id the chain the operation is on
 * @return whether there is an operation, and it is on a chain: a typed message whose domain has
 *         none is on none
 */
bool get_current_operation_chain_id(uint64_t *chain_id);
/**
 * @return the sender of the operation (ADDRESS_LENGTH bytes), NULL if there is none
 */
const uint8_t *get_current_operation_from(void);
/**
 * @return the callee of the operation (ADDRESS_LENGTH bytes), NULL if there is none
 */
const uint8_t *get_current_operation_to(void);
/**
 * @return the native amount the operation sends (INT256_LENGTH bytes), NULL if there is none: a
 *         typed message sends no amount
 */
const uint8_t *get_current_operation_amount(void);
/**
 * @return the running hash of the descriptors received for the operation being described
 */
cx_hash_t *get_current_operation_fields_hash_ctx(void);
/**
 * Get the key signed metadata (enum values, map entries) is attached to, for the operation being
 * described: the chain, the callee and the function selector of the calldata for a call, the domain
 * separator and the hash of the primary type for a typed message
 *
 * @param[out] key the key and which kind it is
 * @return whether there is an operation, and it has a key: a call on no chain has none
 */
bool get_current_operation_key(s_operation_key *key);

bool validate_instruction_hash(void);
bool operation_ctx_pop(void);
bool process_empty_calls_before(void);
bool process_empty_calls_after(void);
/**
 * Process every call left, when none of them has calldata to be described
 *
 * @return false if a call is left that waits for its description, or on failure
 */
bool process_remaining_empty_calls(void);
bool find_matching_operation_ctx(const uint8_t *contract_addr,
                                 const uint8_t *selector,
                                 const uint64_t *chain_id);
/**
 * Attach the info that was matched to the call being described
 *
 * @param[in] info the info of the call, which the call owns if this succeeds
 * @return whether a call is being described, and the info was attached to it
 */
bool operation_ctx_set_info(s_operation_info *info);
/**
 * Make an accepted EIP712_MESSAGE_INFO the root of the operations
 *
 * The message is the operation being described from then on, until a call it makes is. The calls it
 * makes take its sender, and its chain, by default.
 *
 * @param[in] info the message info, which this takes over whether it succeeds or not
 * @param[in] from the account that signs the message
 * @param[in] contract the verifyingContract of the domain, NULL if it has none
 * @param[in] chain_id the chainId of the domain, NULL if it has none
 * @return whether the message was made the root
 */
bool operation_ctx_init_message(s_operation_info *info,
                                const uint8_t from[ADDRESS_LENGTH],
                                const uint8_t contract[ADDRESS_LENGTH],
                                const uint64_t *chain_id);
/**
 * @return whether the root is a typed message
 */
bool operation_ctx_has_message(void);
/**
 * @return whether the message info announces descriptors, which are still to be checked
 */
bool operation_ctx_message_has_pending_fields(void);
/**
 * Whether the descriptors received are the ones the message info announces
 *
 * The announcement is the hash over every descriptor, in the order they came: neither a missing,
 * an additional, a reordered nor an altered descriptor matches it.
 *
 * @return whether the message info announces descriptors, and the ones received are them
 */
bool operation_ctx_message_fields_match(void);
bool operation_ctx_init(s_calldata *calldata,
                        const uint8_t *from,
                        const uint8_t *to,
                        const uint8_t *amount,
                        const uint64_t *chain_id);
void gcs_cleanup(void);
/**
 * Forget the operations, along with what describes them
 */
void operation_ctx_cleanup(void);
void operation_ctx_set_root_value_shown(void);
bool operation_ctx_is_root_value_shown(void);
void operation_ctx_set_root_from_shown(void);
bool operation_ctx_is_root_from_shown(void);
