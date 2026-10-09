#include <string.h>
#include "operation_key.h"
#include "proxy_info.h"

bool operation_key_matches(const s_operation_key *attached, const s_operation_key *operation) {
    const uint8_t *callee;

    if ((attached == NULL) || (operation == NULL) || (attached->context != operation->context)) {
        return false;
    }
    switch (operation->context) {
        case OPERATION_INFO_CTX_SIGN_TX:
            if ((attached->chain_id != operation->chain_id) ||
                (memcmp(attached->selector, operation->selector, sizeof(attached->selector)) !=
                 0)) {
                return false;
            }
            callee = get_implem_contract(&operation->chain_id,
                                         operation->contract_addr,
                                         operation->selector);
            if (callee == NULL) {
                callee = operation->contract_addr;
            }
            return memcmp(callee, attached->contract_addr, sizeof(attached->contract_addr)) == 0;
        case OPERATION_INFO_CTX_EIP712:
            return (memcmp(attached->domain_separator,
                           operation->domain_separator,
                           sizeof(attached->domain_separator)) == 0) &&
                   (memcmp(attached->primary_type_hash,
                           operation->primary_type_hash,
                           sizeof(attached->primary_type_hash)) == 0);
        default:
            return false;
    }
}
