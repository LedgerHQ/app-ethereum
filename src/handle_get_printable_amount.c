#include "handle_get_printable_amount.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"
#include "uint256.h"
#include "string.h"
#include <stdint.h>
#include <os.h>

int handle_get_printable_amount(get_printable_amount_parameters_t* params, chain_config_t* config) {
    uint8_t decimals;
    char ticker[MAX_TICKER_LEN];
    memset(params->printable_amount, 0, sizeof(params->printable_amount));
    if (params->amount_length > 32) {
        PRINTF("Amount is too big, 32 bytes max but buffer has %u bytes", params->amount_length);
        return 0;
    }

    // If the amount is a fee, its value is nominated in ETH even if we're doing an ERC20 swap
    if (params->is_fee) {
        uint8_t ticker_len = strnlen(config->coinName, sizeof(config->coinName));
        memcpy(ticker, config->coinName, ticker_len);
        ticker[ticker_len] = '\0';
        decimals = WEI_TO_ETHER;
    } else {
        // If the amount is *not* a fee, decimals and ticker are built from the given config
        if (!parse_swap_config(params->coin_configuration,
                               params->coin_configuration_length,
                               ticker,
                               &decimals)) {
            PRINTF("Error while parsing config\n");
            return 0;
        }
    }

    amountToString(params->amount,
                   params->amount_length,
                   decimals,
                   ticker,
                   params->printable_amount,
                   sizeof(params->printable_amount));
    return 1;
}
