#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_flow.h"
#include "tokens.h"

#define CONTRACT_ADDR_SIZE 20
#define SELECTOR_SIZE      4

void handleSetExternalPlugin(uint8_t p1,
                             uint8_t p2,
                             uint8_t *workBuffer,
                             uint16_t dataLength,
                             unsigned int *flags,
                             unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    PRINTF("Handling set External Plugin\n");
    uint8_t hash[32];
    cx_ecfp_public_key_t tokenKey;
    uint8_t pluginNameLength = *workBuffer;
    const size_t payload_size = 1 + pluginNameLength + CONTRACT_ADDR_SIZE + SELECTOR_SIZE;

    if (dataLength <= payload_size) {
        THROW(0x6A80);
    }

    if (pluginNameLength + 1 > sizeof(dataContext.tokenContext.pluginName)) {
        THROW(0x6A80);
    }

    // check Ledger's signature over the payload
    cx_hash_sha256(workBuffer, payload_size, hash, sizeof(hash));
    cx_ecfp_init_public_key(CX_CURVE_256K1,
                            LEDGER_SIGNATURE_PUBLIC_KEY,
                            sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                            &tokenKey);
    if (!cx_ecdsa_verify(&tokenKey,
                         CX_LAST,
                         CX_SHA256,
                         hash,
                         sizeof(hash),
                         workBuffer + payload_size,
                         dataLength - payload_size)) {
        PRINTF("Invalid external plugin signature %.*H\n", payload_size, workBuffer);
        THROW(0x6A80);
    }

    // move on to the rest of the payload parsing
    workBuffer++;
    memmove(dataContext.tokenContext.pluginName, workBuffer, pluginNameLength);
    dataContext.tokenContext.pluginName[pluginNameLength] = '\0';
    workBuffer += pluginNameLength;

    PRINTF("Check external plugin %s\n", dataContext.tokenContext.pluginName);

    // Check if the plugin is present on the device
    uint32_t params[2];
    params[0] = (uint32_t) dataContext.tokenContext.pluginName;
    params[1] = ETH_PLUGIN_CHECK_PRESENCE;
    BEGIN_TRY {
        TRY {
            os_lib_call(params);
        }
        CATCH_OTHER(e) {
            PRINTF("%s external plugin is not present\n", dataContext.tokenContext.pluginName);
            memset(dataContext.tokenContext.pluginName,
                   0,
                   sizeof(dataContext.tokenContext.pluginName));
            THROW(0x6984);
        }
        FINALLY {
        }
    }
    END_TRY;

    PRINTF("Plugin found\n");

    memmove(dataContext.tokenContext.contract_address, workBuffer, CONTRACT_ADDR_SIZE);
    workBuffer += 20;
    memmove(dataContext.tokenContext.method_selector, workBuffer, SELECTOR_SIZE);
    externalPluginIsSet = true;

    G_io_apdu_buffer[(*tx)++] = 0x90;
    G_io_apdu_buffer[(*tx)++] = 0x00;
}