#include "handle_swap_sign_transaction.h"
#include "usbd_core.h"
#include "ux.h"
#include "shared_context.h"
#include "utils.h"

bool copy_transaction_parameters(create_transaction_parameters_t* sign_transaction_params,
                                 chain_config_t* config) {
    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with app-ethereum globals
    txStringProperties_t stack_data;
    memset(&stack_data, 0, sizeof(stack_data));
    strncpy(stack_data.fullAddress,
            sign_transaction_params->destination_address,
            sizeof(stack_data.fullAddress));
    if ((stack_data.fullAddress[sizeof(stack_data.fullAddress) - 1] != '\0') ||
        (sign_transaction_params->amount_length > 32) ||
        (sign_transaction_params->fee_amount_length > 8)) {
        return false;
    }

    uint8_t decimals;
    char ticker[MAX_TICKER_LEN];
    if (!parse_swap_config(sign_transaction_params->coin_configuration,
                           sign_transaction_params->coin_configuration_length,
                           ticker,
                           &decimals)) {
        PRINTF("Error while parsing config\n");
        return false;
    }
    amountToString(sign_transaction_params->amount,
                   sign_transaction_params->amount_length,
                   decimals,
                   ticker,
                   stack_data.fullAmount,
                   sizeof(stack_data.fullAmount));

    // If the amount is a fee, its value is nominated in ETH even if we're doing an ERC20 swap
    strlcpy(ticker, config->coinName, MAX_TICKER_LEN);
    decimals = WEI_TO_ETHER;
    amountToString(sign_transaction_params->fee_amount,
                   sign_transaction_params->fee_amount_length,
                   decimals,
                   ticker,
                   stack_data.maxFee,
                   sizeof(stack_data.maxFee));

    memcpy(&strings.common, &stack_data, sizeof(stack_data));
    return true;
}

void handle_swap_sign_transaction(chain_config_t* config) {
    chainConfig = config;
    reset_app_context();
    called_from_swap = true;
    io_seproxyhal_init();

    if (N_storage.initialized != 0x01) {
        internalStorage_t storage;
        storage.dataAllowed = 0x00;
        storage.contractDetails = 0x00;
        storage.initialized = 0x01;
        storage.displayNonce = 0x00;
        nvm_write((void*) &N_storage, (void*) &storage, sizeof(internalStorage_t));
    }

    UX_INIT();
    USB_power(0);
    USB_power(1);
    // ui_idle();
    PRINTF("USB power ON/OFF\n");
#ifdef TARGET_NANOX
    // grab the current plane mode setting
    G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif  // TARGET_NANOX
#ifdef HAVE_BLE
    BLE_power(0, NULL);
    BLE_power(1, "Nano X");
#endif  // HAVE_BLE
    app_main();
}