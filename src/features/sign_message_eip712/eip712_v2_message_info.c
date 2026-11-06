#include <string.h>
#include "eip712_v2_message_info.h"
#include "os_print.h"
#include "common_utils.h"    // ADDRESS_LENGTH
#include "chain_config.h"    // MAX_VALID_CHAIN_ID
#include "shared_context.h"  // tmpCtx
#include "get_public_key.h"  // get_address_of_path
#include "typed_data.h"
#include "type_hash.h"
#include "operation_ctx.h"
#include "proxy_info.h"

/**
 * Get the chain of the domain, when it has one the app supports
 *
 * @param[out] chain_id the chain
 * @return whether the domain has a chainId, an unsigned integer that is a valid chain
 */
static bool get_domain_chain_id(uint64_t *chain_id) {
    return td_get_domain_chain_id(chain_id) && (*chain_id != 0) &&
           (*chain_id <= MAX_VALID_CHAIN_ID);
}

bool eip712_v2_is_domain_separator(const uint8_t separator[CX_KECCAK_256_SIZE]) {
    uint8_t hash[CX_KECCAK_256_SIZE];
    uint8_t contract[ADDRESS_LENGTH];
    uint64_t chain_id;
    const uint8_t *implem;

    if (!td_hash_domain(hash)) {
        return false;
    }
    if (memcmp(hash, separator, sizeof(hash)) == 0) {
        return true;
    }
    // A contract that delegates to another one is described as that one, on the same chain. Only
    // this comparison sees the other contract: the message is hashed with its own.
    if (!get_domain_chain_id(&chain_id) || !td_get_domain_verifying_address(contract) ||
        ((implem = get_implem_contract(&chain_id, contract, NULL)) == NULL)) {
        return false;
    }
    return td_hash_domain_with_contract(implem, hash) &&
           (memcmp(hash, separator, sizeof(hash)) == 0);
}

/**
 * Check a message info is about the message it comes with
 *
 * The domain separator and the primary type hash have to be the ones of the message: the domain
 * separator covers every field of the domain and its type, its chain and its contract included,
 * see \ref eip712_v2_is_domain_separator. A chain or a contract the domain has also has to be one
 * the app can tell: a chainId that is not a valid chain, a verifyingContract that is not an
 * address, would leave the review without them.
 *
 * @param[in] info the message info
 * @return whether it matches the message
 */
static bool is_bound_to_message(const s_operation_info *info) {
    const char *type_name = td_get_message_type_name();
    const uint8_t *expected_type_hash = get_primary_type_hash(info);
    const uint8_t *expected_domain_separator = get_domain_separator(info);
    uint8_t type_hash_buf[CX_KECCAK_256_SIZE];
    uint64_t domain_chain_id;
    uint8_t domain_contract[ADDRESS_LENGTH];

    if ((type_name == NULL) || (expected_type_hash == NULL) ||
        (expected_domain_separator == NULL)) {
        return false;
    }
    if (!type_hash(type_name, type_hash_buf) ||
        (memcmp(type_hash_buf, expected_type_hash, sizeof(type_hash_buf)) != 0)) {
        PRINTF("EIP712 v2: message info is for another type\n");
        return false;
    }
    if (!eip712_v2_is_domain_separator(expected_domain_separator)) {
        PRINTF("EIP712 v2: message info is for another domain\n");
        return false;
    }
    if (td_domain_has_member("chainId") && !get_domain_chain_id(&domain_chain_id)) {
        PRINTF("EIP712 v2: the chainId of the domain is not a chain\n");
        return false;
    }
    if (td_domain_has_member("verifyingContract") &&
        !td_get_domain_contract_addr(domain_contract)) {
        PRINTF("EIP712 v2: the verifyingContract of the domain is not a contract\n");
        return false;
    }
    return true;
}

bool handle_eip712_v2_message_info_struct(const buffer_t *buf) {
    s_operation_info *info;
    uint8_t from[ADDRESS_LENGTH];
    uint8_t contract[ADDRESS_LENGTH];
    uint64_t chain_id;

    if ((info = new_operation_info(buf, OPERATION_INFO_CTX_EIP712)) == NULL) {
        return false;
    }
    if (!is_bound_to_message(info)) {
        delete_operation_info(info);
        return false;
    }
    // the message is signed with the path that came with its values
    if (!get_address_of_path(&tmpCtx.messageSigningContext712.bip32, from)) {
        PRINTF("EIP712 v2: could not derive the signer\n");
        delete_operation_info(info);
        return false;
    }
    // the message is on the chain of its domain, and its calls go to the contract of its domain,
    // when that is an address
    return operation_ctx_init_message(info,
                                      from,
                                      td_get_domain_verifying_address(contract) ? contract : NULL,
                                      get_domain_chain_id(&chain_id) ? &chain_id : NULL);
}
