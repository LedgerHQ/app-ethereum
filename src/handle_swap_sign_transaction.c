#include "handle_swap_sign_transaction.h"
#include "usbd_core.h"
#include "ux.h"
#include "shared_context.h"


void copy_transaction_parameters(create_transaction_parameters_t* sign_transaction_params) {
    // first copy parameters to stack, and then to global data.
    // We need this "trick" as the input data position can overlap with app-ethereum globals
    swap_data_t stack_data;
    memset(&stack_data, 0, sizeof(stack_data));
    strncpy(stack_data.destination_address, sign_transaction_params->destination_address, sizeof(stack_data.destination_address));
    if ((stack_data.destination_address[sizeof(stack_data.destination_address) - 1] != '\0') || 
        (sign_transaction_params->amount_length > 8) ||
        (sign_transaction_params->fee_amount_length > 8)) {
        os_lib_end();
    }
    memcpy(stack_data.amount, sign_transaction_params->amount, sign_transaction_params->amount_length);
    memcpy(stack_data.fees, sign_transaction_params->fee_amount, sign_transaction_params->fee_amount_length);
    stack_data.was_address_checked = 0;
    memcpy(&strings.swap_data, &stack_data, sizeof(stack_data));
}

void handle_swap_sign_transaction(create_transaction_parameters_t* sign_transaction_params, chain_config_t *config) {
    copy_transaction_parameters(sign_transaction_params);
    chainConfig = config;
    reset_app_context();
    called_from_swap = true;
    io_seproxyhal_init();

    if (N_storage.initialized != 0x01) {
    internalStorage_t storage;
    storage.dataAllowed = 0x00;
    storage.contractDetails = 0x00;
    storage.initialized = 0x01;
    nvm_write((void*)&N_storage, (void*)&storage, sizeof(internalStorage_t));
    }
    dataAllowed = N_storage.dataAllowed;
    contractDetails = N_storage.contractDetails;

    UX_INIT();
    USB_power(0);
    USB_power(1);
    //ui_idle();
    PRINTF("USB power ON/OFF\n");
#ifdef TARGET_NANOX
    // grab the current plane mode setting
    G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif // TARGET_NANOX
#ifdef HAVE_BLE
    BLE_power(0, NULL);
    BLE_power(1, "Nano X");
#endif // HAVE_BLE
    app_main();
}