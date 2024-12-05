#include "handle_check_address.h"
#include "apdu_constants.h"
#include "os.h"
#include "shared_context.h"
#include "string.h"
#include "crypto_helpers.h"

#define ZERO(x) explicit_bzero(&x, sizeof(x))

uint16_t handle_check_address(check_address_parameters_t* params, chain_config_t* chain_config) {
    params->result = 0;
    PRINTF("Params on the address %d\n", (unsigned int) params);
    PRINTF("Address to check %s\n", params->address_to_check);
    PRINTF("Inside handle_check_address\n");
    if (params->address_to_check == 0) {
        PRINTF("Address to check == 0\n");
        return APDU_RESPONSE_OK;
    }

    char address[51];
    uint8_t raw_pubkey[65];
    cx_err_t error = CX_INTERNAL_ERROR;
    bip32_path_t bip32;
    bip32.length = params->address_parameters[0];
    if (bip32_path_read(params->address_parameters + 1,
                        params->address_parameters_length,
                        bip32.path,
                        bip32.length) == false) {
        PRINTF("Invalid path\n");
        return APDU_RESPONSE_INVALID_DATA;
    }
    CX_CHECK(bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                         bip32.path,
                                         bip32.length,
                                         raw_pubkey,
                                         NULL,
                                         CX_SHA512));

    getEthAddressStringFromRawKey((const uint8_t*) raw_pubkey, address, chain_config->chainId);

    uint8_t offset_0x = 0;
    if (memcmp(params->address_to_check, "0x", 2) == 0) {
        offset_0x = 2;
    }

    if (strcmp(address, params->address_to_check + offset_0x) != 0) {
        PRINTF("Addresses don't match\n");
    } else {
        PRINTF("Addresses match\n");
        params->result = 1;
    }
    error = APDU_RESPONSE_OK;
end:
    return error;
}
