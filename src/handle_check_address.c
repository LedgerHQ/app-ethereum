#include "handle_check_address.h"
#include "os.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "string.h"

void handle_check_address(check_address_parameters_t* params, chain_config_t* chain_config) {
    PRINTF("Params on the address %d\n",(unsigned int)params);
    PRINTF("Address to check %s\n",params->address_to_check);
    PRINTF("Inside handle_check_address\n");
    params->result = 0;
    if (params->address_to_check == 0) {
        PRINTF("Address to check == 0\n");
        return;
    }
    
    char address[51];
    os_memset(address, 0, 51);
    uint8_t privateKeyData[32];
    uint32_t bip32Path[MAX_BIP32_PATH];
    uint32_t i;
    uint8_t *bip32_path_ptr = params->address_parameters;
    uint8_t bip32PathLength = *(bip32_path_ptr++);
    cx_ecfp_private_key_t privateKey;
    cx_ecfp_public_key_t publicKey;
    cx_sha3_t sha3;
    if ((bip32PathLength < 0x01) ||
        (bip32PathLength > MAX_BIP32_PATH) ||
        (bip32PathLength*4 != params->address_parameters_length - 1)) {
        PRINTF("Invalid path\n");
        return;
    }
    for (i = 0; i < bip32PathLength; i++) {
        bip32Path[i] = U4BE(bip32_path_ptr, 0);
        bip32_path_ptr += 4;
    }
    os_perso_derive_node_bip32(CX_CURVE_256K1, bip32Path, bip32PathLength, privateKeyData, NULL);
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    cx_ecfp_generate_pair(CX_CURVE_256K1, &publicKey, &privateKey, 1);
    os_memset(&privateKey, 0, sizeof(privateKey));
    os_memset(privateKeyData, 0, sizeof(privateKeyData));
    getEthAddressStringFromKey(&publicKey, (uint8_t*)address, &sha3, chain_config);
    
    uint8_t offset_0x = 0;
    if(memcmp(params->address_to_check, "0x", 2) == 0){
        offset_0x = 2;
    }

    if ((strlen(address) != strlen(params->address_to_check + offset_0x)) ||
        os_memcmp(address, params->address_to_check + offset_0x, strlen(address)) != 0) {
        PRINTF("Addresses doesn't match\n");
        return;
    }
    PRINTF("Addresses  match\n");
    params->result = 1;
}