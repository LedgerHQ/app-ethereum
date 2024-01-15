#include "handle_check_address.h"
#include "os.h"
#include "shared_context.h"
#include "string.h"

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
    cx_sha3_t local_sha3;

    // Common memory is used for locals that are not used concurrently
    union group1 {
        uint32_t bip32Path[MAX_BIP32_PATH];
        cx_ecfp_private_key_t privateKey;
        char address[51];
    } locals_union1;
    union group2 {
        uint8_t privateKeyData[64];
        cx_ecfp_public_key_t publicKey;
    } locals_union2;

    if ((bip32PathLength < 0x01) || (bip32PathLength > MAX_BIP32_PATH) ||
        (bip32PathLength * 4 != params->address_parameters_length - 1)) {
        PRINTF("Invalid path\n");
        return;
    }
    for (uint8_t i = 0; i < bip32PathLength; i++) {
        locals_union1.bip32Path[i] = U4BE(bip32_path_ptr, 0);
        bip32_path_ptr += 4;
    }
    if (os_derive_bip32_no_throw(CX_CURVE_256K1,
                                 locals_union1.bip32Path,
                                 bip32PathLength,
                                 locals_union2.privateKeyData,
                                 NULL) != CX_OK) {
        ZERO(locals_union1);
        ZERO(locals_union2);
        return;
    }

    ZERO(locals_union1);
    if (cx_ecfp_init_private_key_no_throw(CX_CURVE_256K1,
                                          locals_union2.privateKeyData,
                                          32,
                                          &locals_union1.privateKey) != CX_OK) {
        ZERO(locals_union1);
        ZERO(locals_union2);
        return;
    }
    ZERO(locals_union2);
    if (cx_ecfp_generate_pair_no_throw(CX_CURVE_256K1,
                                       &locals_union2.publicKey,
                                       &locals_union1.privateKey,
                                       1) != CX_OK) {
        ZERO(locals_union1);
        ZERO(locals_union2);
        return;
    }
    ZERO(locals_union1);
    if (!getEthAddressStringFromKey(&locals_union2.publicKey,
                                    locals_union1.address,
                                    &local_sha3,
                                    chain_config->chainId)) {
        ZERO(locals_union1);
        ZERO(locals_union2);
        return;
    }
    ZERO(locals_union2);

    uint8_t offset_0x = 0;
    if (memcmp(params->address_to_check, "0x", 2) == 0) {
        offset_0x = 2;
    }

    if (strcmp(locals_union1.address, params->address_to_check + offset_0x) != 0) {
        PRINTF("Addresses don't match\n");
    } else {
        PRINTF("Addresses match\n");
        params->result = 1;
    }
    ZERO(locals_union1);
}
