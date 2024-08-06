#include <stdint.h>
#include <os.h>

#include "eth_swap_utils.h"
#include "handle_get_printable_amount.h"
#include "shared_context.h"
#include "common_utils.h"
#include "uint256.h"
#include "string.h"
#include "network.h"
#include "apdu_constants.h"

uint32_t handle_get_printable_amount(get_printable_amount_parameters_t* params,
                                     chain_config_t* config) {
    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;
    uint64_t chain_id = 0;

    memset(params->printable_amount, 0, sizeof(params->printable_amount));
    if (params->amount_length > 32) {
        PRINTF("Amount is too big, 32 bytes max but buffer has %u bytes", params->amount_length);
        return APDU_RESPONSE_INVALID_DATA;
    }

    if (!parse_swap_config(params->coin_configuration,
                           params->coin_configuration_length,
                           ticker,
                           &decimals,
                           &chain_id)) {
        PRINTF("Error while parsing config\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    // If the amount is a fee, the ticker should be the chain's native currency
    if (params->is_fee) {
        // fallback mechanism in the absence of chain ID in swap config
        if (chain_id == 0) {
            chain_id = config->chainId;
        }
        strlcpy(ticker, get_displayable_ticker(&chain_id, config), sizeof(ticker));
        decimals = WEI_TO_ETHER;
    }

    if (!amountToString(params->amount,
                        params->amount_length,
                        decimals,
                        ticker,
                        params->printable_amount,
                        sizeof(params->printable_amount))) {
        memset(params->printable_amount, 0, sizeof(params->printable_amount));
    }
    return APDU_RESPONSE_OK;
}
