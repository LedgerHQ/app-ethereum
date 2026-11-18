#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cx.h"
#include "calldata.h"      // CALLDATA_SELECTOR_SIZE
#include "common_utils.h"  // ADDRESS_LENGTH, INT256_LENGTH
#include "buffer.h"
#include "lcx_ecdsa.h"
#include "lists.h"
#include "tlv_library.h"
#include "tlv_utils.h"  // tlv_get_hash

#define OPERATION_TYPE_SIZE     31
#define CREATOR_NAME_SIZE       23
#define CREATOR_LEGAL_NAME_SIZE 31
#define CREATOR_URL_SIZE        27
#define CONTRACT_NAME_SIZE      31
#define DEPLOY_DATE_SIZE        11  // "YYYY-MM-DD\0" -> 4 + 1 + 2 + 1 + 2 + 1

// The signed header of a clear-signed operation, shared by TRANSACTION_INFO and
// EIP712_MESSAGE_INFO. Which of the two it was received as: both share one wire format, the tags
// that identify what is signed and the optional tags aside. SIGN_TX is the zero value.
typedef enum {
    OPERATION_INFO_CTX_SIGN_TX = 0,  // TRANSACTION_INFO
    OPERATION_INFO_CTX_EIP712,       // EIP712_MESSAGE_INFO
} e_operation_info_context;

typedef struct {
    e_operation_info_context context;
    uint8_t version;
    union {
        struct {  // SIGN_TX
            uint64_t chain_id;
            uint8_t contract_addr[ADDRESS_LENGTH];
            uint8_t selector[CALLDATA_SELECTOR_SIZE];
        };
        struct {  // EIP712, whose domain separator covers the chain and the contract
            uint8_t domain_separator[INT256_LENGTH];
            uint8_t primary_type_hash[INT256_LENGTH];
        };
    };
    uint8_t fields_hash[INT256_LENGTH];
    char operation_type[OPERATION_TYPE_SIZE];
    char creator_name[CREATOR_NAME_SIZE];
    char creator_legal_name[CREATOR_LEGAL_NAME_SIZE];
    char creator_url[CREATOR_URL_SIZE];
    char contract_name[CONTRACT_NAME_SIZE];
    char deploy_date[DEPLOY_DATE_SIZE];
    uint8_t signature_len;
    uint8_t signature[CX_ECDSA_SHA256_SIG_MAX_ASN1_LENGTH];
} s_operation_info;

/**
 * Create an operation info from a TRANSACTION_INFO or EIP712_MESSAGE_INFO struct: parse it and
 * verify its required tags and signature
 *
 * @param[in] buf the complete struct payload
 * @param[in] context which of the two structs it is
 * @return a new operation info the caller owns (see \ref delete_operation_info), NULL on failure
 */
s_operation_info *new_operation_info(const buffer_t *buf, e_operation_info_context context);

/**
 * Whether an operation info announces no field descriptors at all, that is whether its fields hash
 * is the hash of an empty input
 *
 * @param[in] info the operation info
 * @param[out] is_empty whether the fields hash is the empty hash
 * @return whether the hash could be computed
 */
bool operation_info_fields_hash_is_empty(const s_operation_info *info, bool *is_empty);

const char *get_operation_type(const s_operation_info *info);
const char *get_creator_name(const s_operation_info *info);
const char *get_creator_legal_name(const s_operation_info *info);
const char *get_creator_url(const s_operation_info *info);
const char *get_contract_name(const s_operation_info *info);
const uint8_t *get_selector(const s_operation_info *info);
const uint8_t *get_primary_type_hash(const s_operation_info *info);
const uint8_t *get_domain_separator(const s_operation_info *info);
const char *get_deploy_date(const s_operation_info *info);
void delete_operation_info(s_operation_info *node);
