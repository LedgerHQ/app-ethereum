#include "os_io_seproxyhal.h"
#include "os.h"
#include "ux.h"
#include "eth_swap_utils.h"
#include "handle_swap_sign_transaction.h"
#include "shared_context.h"
#include "common_utils.h"
#include "network.h"
#include "cmd_setPlugin.h"
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
    uint8_t swap_crosschain_hash[sizeof(G_swap_crosschain_hash)];
    swap_mode_t swap_mode;

    explicit_bzero(&stack_data, sizeof(stack_data));
    explicit_bzero(destination_address_extra_data, sizeof(destination_address_extra_data));
    explicit_bzero(swap_crosschain_hash, sizeof(swap_crosschain_hash));

    // Set destination address
    strlcpy(stack_data.toAddress,
            sign_transaction_params->destination_address,
            sizeof(stack_data.toAddress));
    if ((stack_data.toAddress[sizeof(stack_data.toAddress) - 1] != '\0') ||
        (sign_transaction_params->amount_length > 32) ||
        (sign_transaction_params->fee_amount_length > 8)) {
        return false;
    }
    PRINTF("Expecting destination_address %s\n", stack_data.toAddress);

    if (sign_transaction_params->destination_address_extra_id != NULL) {
        memcpy(destination_address_extra_data,
               sign_transaction_params->destination_address_extra_id,
               sizeof(destination_address_extra_data));
    }

    // if destination_address_extra_id is given, we use the first byte to determine if we use the
    // normal swap protocol, or the one for cross-chain swaps
    switch (destination_address_extra_data[0]) {
        case EXTRA_ID_TYPE_NATIVE:
            // we don't use the payin_extra_id field in this mode
            swap_mode = SWAP_MODE_STANDARD;
            PRINTF("Standard swap\n");
            break;
        case EXTRA_ID_TYPE_EVM_CALLDATA:
            swap_mode = SWAP_MODE_CROSSCHAIN_PENDING_CHECK;

            memcpy(swap_crosschain_hash,
                   destination_address_extra_data + 1,
                   sizeof(swap_crosschain_hash));

            PRINTF("Crosschain swap with hash: %.*H\n", CX_SHA256_SIZE, swap_crosschain_hash);
            break;
        default:
            // We can't return errors from here, we remember that we have an issue to report later
            PRINTF("Invalid or unknown swap protocol\n");
            swap_mode = SWAP_MODE_ERROR;
    }

    char asset_ticker[MAX_TICKER_LEN];
    uint8_t asset_decimals;
    uint64_t chain_id = 0;

    if (!parse_swap_config(sign_transaction_params->coin_configuration,
                           sign_transaction_params->coin_configuration_length,
                           asset_ticker,
                           &asset_decimals,
                           &chain_id)) {
        PRINTF("Error while parsing config\n");
        return false;
    }

    // fallback mechanism in the absence of chain ID in swap config
    if (chain_id == 0) {
        chain_id = config->chainId;
    }
    PRINTF("chain_id = %d\n", (uint32_t) chain_id);

    // If the amount is a fee, its value is nominated in NATIVE even if we're doing an ERC20 swap
    const char* native_ticker = get_displayable_ticker(&chain_id, config);
    uint8_t native_decimals = WEI_TO_ETHER;
    if (!amountToString(sign_transaction_params->fee_amount,
                        sign_transaction_params->fee_amount_length,
                        native_decimals,
                        native_ticker,
                        stack_data.maxFee,
                        sizeof(stack_data.maxFee))) {
        return false;
    }
    PRINTF("Expecting fees %s\n", stack_data.maxFee);

    if (swap_mode == SWAP_MODE_CROSSCHAIN_PENDING_CHECK &&
        strcmp(native_ticker, asset_ticker) != 0) {
        // Special case: crosschain swap of non native assets (tokens)
        uint8_t zero_amount = 0;
        if (!amountToString(&zero_amount,
                            1,
                            native_decimals,
                            native_ticker,
                            stack_data.fullAmount,
                            sizeof(stack_data.fullAmount))) {
            return false;
        }
    } else {
        if (!amountToString(sign_transaction_params->amount,
                            sign_transaction_params->amount_length,
                            asset_decimals,
                            asset_ticker,
                            stack_data.fullAmount,
                            sizeof(stack_data.fullAmount))) {
            return false;
        }
    }
    PRINTF("Expecting amount %s\n", stack_data.fullAmount);

    // Full reset the global variables
    os_explicit_zero_BSS_segment();
    // Keep the address at which we'll reply the signing status
    G_swap_sign_return_value_address = &sign_transaction_params->result;
    // Commit the values read from exchange to the clean global space
    G_swap_mode = swap_mode;
    memcpy(G_swap_crosschain_hash, swap_crosschain_hash, sizeof(G_swap_crosschain_hash));
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
    // If we are in crosschain context, automatically register the CROSSCHAIN plugin
    if (G_swap_mode == SWAP_MODE_CROSSCHAIN_PENDING_CHECK) {
        set_swap_with_calldata_plugin_type();
    }

    common_app_init();

    storage_init();

#ifdef HAVE_NBGL
    nbgl_useCaseSpinner("Signing");
#endif  // HAVE_NBGL

    app_main();

    // Failsafe
    app_exit();
    while (1)
        ;
}
