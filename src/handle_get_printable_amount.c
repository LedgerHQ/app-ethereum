#include "handle_get_printable_amount.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"
#include "uint256.h"
#include "string.h"
#include <stdint.h>


void handle_get_printable_amount( get_printable_amount_parameters_t* params, chain_config_t *config) {
    os_memset(params->printable_amount, 0, sizeof(params->printable_amount));
    if (params->amount_length > 32) {
        PRINTF("Amount is too big, 32 bytes max but buffer has %u bytes", params->amount_length);
        return;
    }
    
    unsigned char coin_name_length = strlen(config->coinName);
    os_memmove(params->printable_amount, config->coinName, coin_name_length);

    uint256_t uint256;
    char amount_ascii[30];
    convertUint256BE(params->amount, params->amount_length, &uint256);
    tostring256(&uint256, 10, amount_ascii, sizeof(amount_ascii));
    uint8_t amount_ascii_length = strnlen(amount_ascii, sizeof(amount_ascii));
    adjustDecimals(amount_ascii, amount_ascii_length, params->printable_amount + coin_name_length, sizeof(params->printable_amount) - coin_name_length, WEI_TO_ETHER);
}