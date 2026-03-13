#include "os.h"
#include "eth_swap_utils.h"
#include "apdu_constants.h"

void handle_get_printable_amount(get_printable_amount_parameters_t* params,
                                 chain_config_t* config) {
    swap_context_t context = {0};
    char* ticker = NULL;
    uint8_t decimals = 0;

    memset(params->printable_amount, 0, sizeof(params->printable_amount));
    if (params->amount_length > 32) {
        PRINTF("Amount is too big, 32 bytes max but buffer has %u bytes", params->amount_length);
        return;
    }

    if (!parse_swap_config(params->coin_configuration,
                           params->coin_configuration_length,
                           &context)) {
        PRINTF("Error while parsing config\n");
        return;
    }
    // If the amount is a fee, the ticker should be the chain's native currency
    get_asset_info_on_network(params->is_fee, &context, config, &ticker, &decimals);

    if (!amountToString(params->amount,
                        params->amount_length,
                        decimals,
                        ticker,
                        params->printable_amount,
                        sizeof(params->printable_amount))) {
        memset(params->printable_amount, 0, sizeof(params->printable_amount));
    }
}
