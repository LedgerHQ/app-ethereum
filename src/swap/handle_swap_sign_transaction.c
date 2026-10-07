#include "os_lib.h"  // os_lib_end

#include "eth_swap_utils.h"
#include "shared_context.h"
#include "cmd_set_plugin.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "common_ui.h"
#include "os.h"

// Standard or crosschain swap type
swap_mode_t G_swap_mode;

// On crosschain swap, save the hash promised by the partner
uint8_t *G_swap_crosschain_hash = NULL;

uint64_t G_swap_expected_chain_id;
uint8_t G_swap_expected_token_address[ADDRESS_LENGTH];
bool G_swap_has_expected_token_address;
// See declaration in shared_context.h
swap_value_check_t G_swap_expected_value_check;
uint8_t G_swap_expected_value[INT256_LENGTH];

typedef enum extra_id_type_e {
    EXTRA_ID_TYPE_NATIVE,
    EXTRA_ID_TYPE_EVM_CALLDATA,
    // There are others but they are not relevant for the Ethereum application
} extra_id_type_t;

bool copy_transaction_parameters(create_transaction_parameters_t *sign_transaction_params,
                                 const chain_config_t *config) {
    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with app-ethereum globals
    txStringProperties_t stack_data;
    uint8_t destination_address_extra_data[CX_SHA256_SIZE + 1];
    uint8_t swap_crosschain_hash[CX_SHA256_SIZE];
    uint8_t expected_value[INT256_LENGTH];
    swap_mode_t swap_mode;

    explicit_bzero(&stack_data, sizeof(stack_data));
    explicit_bzero(destination_address_extra_data, sizeof(destination_address_extra_data));
    explicit_bzero(swap_crosschain_hash, sizeof(swap_crosschain_hash));
    explicit_bzero(expected_value, sizeof(expected_value));

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

    swap_context_t context = {0};
    char *ticker = NULL;

    if (!parse_swap_config(sign_transaction_params->coin_configuration,
                           sign_transaction_params->coin_configuration_length,
                           &context)) {
        PRINTF("Error while parsing config\n");
        return false;
    }

    if (context.chain_id == 0) {
        // fallback mechanism in the absence of chain ID in swap config
        context.chain_id = config->chain_id;
    }

    // If the amount is a fee, its value is nominated in NATIVE even if we're doing an ERC20 swap
    get_asset_info_on_network(true, &context, (chain_config_t *) config, &ticker, NULL);

    if (!amountToString(sign_transaction_params->fee_amount,
                        sign_transaction_params->fee_amount_length,
                        context.fees_asset_info.decimals,
                        ticker,
                        stack_data.maxFee,
                        sizeof(stack_data.maxFee))) {
        return false;
    }
    PRINTF("Expecting fees %s\n", stack_data.maxFee);

    swap_value_check_t expected_value_check = SWAP_VALUE_CHECK_AMOUNT;
    if (swap_mode == SWAP_MODE_CROSSCHAIN_PENDING_CHECK &&
        (context.has_token_address ||
         context.swapped_asset_info.decimals != context.fees_asset_info.decimals ||
         strcmp(ticker, context.swapped_asset_info.ticker) != 0)) {
        // The swapped asset is a token, distinct from the network's native currency: the config
        // announced its contract address or, for a config predating that optional field, declared
        // decimals or a ticker that the native currency cannot have. Decimals are conclusive
        // because parse_swap_config() pins the fee asset's, hence the network's, to WEI_TO_ETHER,
        // which is also what the signature path formats the transaction value with. The token's
        // real amount lives inside the calldata, not in the transaction's value field (the
        // calldata itself is authenticated separately via G_swap_crosschain_hash), so the expected
        // on-chain value here is always zero, and a non-zero value must be rejected.
        expected_value_check = SWAP_VALUE_CHECK_ZERO;
        uint8_t zero_amount = 0;
        if (!amountToString(&zero_amount,
                            1,
                            context.fees_asset_info.decimals,
                            ticker,
                            stack_data.fullAmount,
                            sizeof(stack_data.fullAmount))) {
            return false;
        }
    } else {
        // No signal identified a token (or this isn't a crosschain swap): the native currency
        // itself is being spent, so the expected on-chain value is the real swapped amount.
        //
        // A token whose config matches the native currency on all three signals at once is
        // indistinguishable from it and lands here too. It is then held to the native rule: its
        // legitimate zero-value form is refused, but a value equal to the validated amount is
        // still accepted, on top of the token the calldata spends. Only the contract address
        // resolves that, so such an asset must not be listed for swap until the config carries
        // it.
        if (!amountToString(sign_transaction_params->amount,
                            sign_transaction_params->amount_length,
                            context.swapped_asset_info.decimals,
                            context.swapped_asset_info.ticker,
                            stack_data.fullAmount,
                            sizeof(stack_data.fullAmount))) {
            return false;
        }
    }
    // Stage the amount as the raw bytes Exchange provided, beside its formatted form: the value
    // check binds these, and reading them here is what the stack copy exists for, since the input
    // data may overlap the globals that os_explicit_zero_BSS_segment() is about to wipe.
    memcpy(expected_value + sizeof(expected_value) - sign_transaction_params->amount_length,
           sign_transaction_params->amount,
           sign_transaction_params->amount_length);
    PRINTF("Expecting amount %s\n", stack_data.fullAmount);

    // Full reset the global variables
    os_explicit_zero_BSS_segment();
    // Keep the address at which we'll reply the signing status
    G_swap_signing_return_value_address = &sign_transaction_params->result;
    // Commit the values read from exchange to the clean global space
    G_swap_mode = swap_mode;
    G_swap_expected_chain_id = context.chain_id;
    G_swap_has_expected_token_address = context.has_token_address;
    if (context.has_token_address) {
        memcpy(G_swap_expected_token_address, context.token_address, ADDRESS_LENGTH);
    }
    G_swap_expected_value_check = expected_value_check;
    memcpy(G_swap_expected_value, expected_value, sizeof(G_swap_expected_value));

    app_mem_init();
    if ((G_swap_crosschain_hash = APP_MEM_ALLOC(CX_SHA256_SIZE)) == NULL) {
        PRINTF("Memory allocation failed for G_swap_crosschain_hash\n");
        return false;
    }
    memcpy(G_swap_crosschain_hash, swap_crosschain_hash, CX_SHA256_SIZE);
    memcpy(&strings.common, &stack_data, sizeof(stack_data));
    return true;
}

void __attribute__((noreturn)) swap_finalize_exchange_sign_transaction(bool is_success) {
    APP_MEM_FREE(G_swap_crosschain_hash);
    G_swap_crosschain_hash = NULL;
    // *G_swap_signing_return_value_address position is arbitrary in eth memory
    *G_swap_signing_return_value_address = is_success;
    os_lib_end();
}

void __attribute__((noreturn)) handle_swap_sign_transaction(const chain_config_t *config) {
    g_chain_config = config;
    G_called_from_swap = true;
    G_swap_response_ready = false;
    // If we are in crosschain context, automatically register the CROSSCHAIN plugin
    if (G_swap_mode == SWAP_MODE_CROSSCHAIN_PENDING_CHECK) {
        set_swap_with_calldata_plugin_type();
    }

    ui_swap_show_signing();

    app_main();

    // Failsafe
    app_quit();
    while (1);
}
