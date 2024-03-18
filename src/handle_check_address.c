#include "handle_check_address.h"
#include "apdu_constants.h"
#include "os.h"
#include "shared_context.h"
#include "string.h"
#include "lib_standard_app/crypto_helpers.h"

#define ZERO(x) explicit_bzero(&x, sizeof(x))

void handle_check_address(check_address_parameters_t* params, chain_config_t* chain_config) {
    params->result = 0;
    PRINTF("Params on the address %d\n", (unsigned int) params);
    PRINTF("Address to check %s\n", params->address_to_check);
    PRINTF("Inside handle_check_address\n");
    if (params->address_to_check == 0) {
        PRINTF("Address to check == 0\n");
        return;
    }

    const uint8_t* bip32_path_ptr = params->address_parameters;
    uint8_t bip32PathLength = *(bip32_path_ptr++);
    uint32_t bip32Path[MAX_BIP32_PATH];
    char address[51];
    uint8_t raw_pubkey[65];

    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH) ||
        (bip32PathLength * 4 != params->address_parameters_length - 1)) {
        PRINTF("Invalid path\n");
        return;
    }
    for (uint8_t i = 0; i < bip32PathLength; i++) {
        bip32Path[i] = U4BE(bip32_path_ptr, 0);
        bip32_path_ptr += 4;
    }

    if (bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                    bip32Path,
                                    bip32PathLength,
                                    raw_pubkey,
                                    NULL,
                                    CX_SHA512) != CX_OK) {
        THROW(APDU_RESPONSE_UNKNOWN);
    }

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
}
