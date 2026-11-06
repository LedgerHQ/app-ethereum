#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cx.h"
#include "nbgl_use_case.h"

// The rows that show the hashes of the message, when the "Transaction hash" setting is on
#define EIP712_DOMAIN_HASH_LABEL  "Domain hash"
#define EIP712_MESSAGE_HASH_LABEL "Message hash"
#define EIP712_HASH_ROWS          2
// "0x", the digits of a hash, and the NUL
#define EIP712_HASH_STR_SIZE (2 * CX_KECCAK_256_SIZE + 3)

bool ui_712_prepare_review(void);
bool ui_712_prepare_blind_review(void);

/**
 * Format the hash of the domain and the hash of the message that are being signed
 *
 * @param[out] domain_str receives the domain hash, of at least EIP712_HASH_STR_SIZE
 * @param[out] message_str receives the message hash, of at least EIP712_HASH_STR_SIZE
 */
void eip712_format_hash_strs(char *domain_str, char *message_str);

/**
 * Fill the rows that show the hashes of the domain and of the message that are being signed
 *
 * @param[out] pairs the EIP712_HASH_ROWS rows
 * @param[out] domain_str receives the domain hash the first row shows, of EIP712_HASH_STR_SIZE
 * @param[out] message_str receives the message hash the second row shows, of EIP712_HASH_STR_SIZE
 */
void eip712_set_hash_pairs(nbgl_contentTagValue_t *pairs, char *domain_str, char *message_str);

bool eip712_format_hash(uint8_t index);
void eip712_hash_strs_cleanup(void);

void ui_712_approve_cb(void);
void ui_712_reject_cb(void);
