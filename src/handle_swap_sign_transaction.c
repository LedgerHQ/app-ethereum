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

// Remember if we have been started by the Exchange application or not
bool G_called_from_swap;

// Set this boolean when a transaction is signed in Swap mode. Safety against double sign
bool G_swap_response_ready;

// Save the BSS address where we will write the return value when finished
static uint8_t* G_swap_sign_return_value_address;

// Standard or crosschain swap type
swap_mode_t G_swap_mode;

// On crosschain swap, save the hash promised by the partner
uint8_t G_swap_crosschain_hash[CX_SHA256_SIZE];

typedef enum extra_id_type_e {
    EXTRA_ID_TYPE_NATIVE,
    EXTRA_ID_TYPE_EVM_CALLDATA,
    // There are others but they are not relevant for the Ethereum application
} extra_id_type_t;

bool copy_transaction_parameters(create_transaction_parameters_t* sign_transaction_params,
                                 const chain_config_t* config) {
    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with app-ethereum globals
    txStringProperties_t stack_data;
    uint8_t destination_address_extra_data[CX_SHA256_SIZE + 1];

    memset(&stack_data, 0, sizeof(stack_data));
    memset(destination_address_extra_data, 0, sizeof(destination_address_extra_data));

    strlcpy(stack_data.toAddress,
            sign_transaction_params->destination_address,
            sizeof(stack_data.toAddress));
    if ((stack_data.toAddress[sizeof(stack_data.toAddress) - 1] != '\0') ||
        (sign_transaction_params->amount_length > 32) ||
        (sign_transaction_params->fee_amount_length > 8)) {
        return false;
    }

    if (sign_transaction_params->destination_address_extra_id != NULL) {
        memcpy(destination_address_extra_data,
               sign_transaction_params->destination_address_extra_id,
               sizeof(destination_address_extra_data));
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

    // if destination_address_extra_id is given, we use the first byte to determine if we use the
    // normal swap protocol, or the one for cross-chain swaps
    switch (destination_address_extra_data[0]) {
        case EXTRA_ID_TYPE_NATIVE:
            G_swap_mode = SWAP_MODE_STANDARD;
            PRINTF("Standard swap\n");

            // we don't use the payin_extra_id field in this mode
            explicit_bzero(G_swap_crosschain_hash, sizeof(G_swap_crosschain_hash));
            break;
        case EXTRA_ID_TYPE_EVM_CALLDATA:
            G_swap_mode = SWAP_MODE_CROSSCHAIN_PENDING_CHECK;

            memcpy(G_swap_crosschain_hash,
                   destination_address_extra_data + 1,
                   sizeof(G_swap_crosschain_hash));

            PRINTF("Crosschain swap with hash: %.*H\n", CX_SHA256_SIZE, G_swap_crosschain_hash);
            break;
        default:
            // We can't return errors from here, we remember that we have an issue to report later
            PRINTF("Invalid or unknown swap protocol\n");
            G_swap_mode = SWAP_MODE_ERROR;
    }

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
        explicit_bzero(&storage, sizeof(storage));
        storage.initialized = true;
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
