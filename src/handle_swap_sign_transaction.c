#include "os_io_seproxyhal.h"
#include "os.h"
#include "ux.h"
#include "eth_swap_utils.h"
#include "handle_swap_sign_transaction.h"
#include "shared_context.h"
#include "common_utils.h"
#include "network.h"
#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif  // HAVE_NBGL

// Save the BSS address where we will write the return value when finished
static uint8_t* G_swap_sign_return_value_address;

bool copy_transaction_parameters(create_transaction_parameters_t* sign_transaction_params,
                                 const chain_config_t* config) {
    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with app-ethereum globals
    txStringProperties_t stack_data;
    memset(&stack_data, 0, sizeof(stack_data));
    strlcpy(stack_data.toAddress,
            sign_transaction_params->destination_address,
            sizeof(stack_data.toAddress));
    if ((stack_data.toAddress[sizeof(stack_data.toAddress) - 1] != '\0') ||
        (sign_transaction_params->amount_length > 32) ||
        (sign_transaction_params->fee_amount_length > 8)) {
        return false;
    }

    char ticker[MAX_TICKER_LEN];
    uint8_t decimals;
    uint64_t chain_id = 0;

    if (!parse_swap_config(sign_transaction_params->coin_configuration,
                           sign_transaction_params->coin_configuration_length,
                           ticker,
                           &decimals,
                           &chain_id)) {
        PRINTF("Error while parsing config\n");
        return false;
    }
    if (!amountToString(sign_transaction_params->amount,
                        sign_transaction_params->amount_length,
                        decimals,
                        ticker,
                        stack_data.fullAmount,
                        sizeof(stack_data.fullAmount))) {
        return false;
    }

    // fallback mechanism in the absence of chain ID in swap config
    if (chain_id == 0) {
        chain_id = config->chainId;
    }
    // If the amount is a fee, its value is nominated in ETH even if we're doing an ERC20 swap
    strlcpy(ticker, get_displayable_ticker(&chain_id, config), sizeof(ticker));
    decimals = WEI_TO_ETHER;
    if (!amountToString(sign_transaction_params->fee_amount,
                        sign_transaction_params->fee_amount_length,
                        decimals,
                        ticker,
                        stack_data.maxFee,
                        sizeof(stack_data.maxFee))) {
        return false;
    }

    // Full reset the global variables
    os_explicit_zero_BSS_segment();
    // Keep the address at which we'll reply the signing status
    G_swap_sign_return_value_address = &sign_transaction_params->result;
    // Commit the values read from exchange to the clean global space

    memcpy(&strings.common, &stack_data, sizeof(stack_data));
    return true;
}

void __attribute__((noreturn)) finalize_exchange_sign_transaction(bool is_success) {
    *G_swap_sign_return_value_address = is_success;
    os_lib_end();
}

void __attribute__((noreturn)) handle_swap_sign_transaction(const chain_config_t* config) {
    chainConfig = config;
    reset_app_context();
    G_called_from_swap = true;
    G_swap_response_ready = false;

    common_app_init();

    if (N_storage.initialized != 0x01) {
        internalStorage_t storage;
        storage.contractDetails = 0x00;
        storage.initialized = 0x01;
        storage.displayNonce = 0x00;
        storage.contractDetails = 0x00;
        nvm_write((void*) &N_storage, (void*) &storage, sizeof(internalStorage_t));
    }

#ifdef HAVE_NBGL
    nbgl_useCaseSpinner("Signing");
#endif  // HAVE_NBGL

    app_main();

    // Failsafe
    app_exit();
    while (1)
        ;
}
