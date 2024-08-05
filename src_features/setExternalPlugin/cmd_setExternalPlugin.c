#include "shared_context.h"
#include "apdu_constants.h"
#include "public_keys.h"
#include "eth_plugin_interface.h"
#include "eth_plugin_internal.h"
#include "plugin_utils.h"
#include "common_ui.h"
#include "os_io_seproxyhal.h"
#ifdef HAVE_LEDGER_PKI
#include "os_pki.h"
#endif

void handleSetExternalPlugin(uint8_t p1,
                             uint8_t p2,
                             const uint8_t *workBuffer,
                             uint8_t dataLength,
                             unsigned int *flags,
                             unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    PRINTF("Handling set Plugin\n");
    uint8_t hash[INT256_LENGTH];
    uint8_t pluginNameLength = *workBuffer;
    uint32_t params[2];
    cx_err_t error = CX_INTERNAL_ERROR;

    PRINTF("plugin Name Length: %d\n", pluginNameLength);
    const size_t payload_size = 1 + pluginNameLength + ADDRESS_LENGTH + SELECTOR_SIZE;

    if (dataLength <= payload_size) {
        PRINTF("data too small: expected at least %d got %d\n", payload_size, dataLength);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    if (pluginNameLength + 1 > sizeof(dataContext.tokenContext.pluginName)) {
        PRINTF("name length too big: expected max %d, got %d\n",
               sizeof(dataContext.tokenContext.pluginName),
               pluginNameLength + 1);
        THROW(APDU_RESPONSE_INVALID_DATA);
    }

    // check Ledger's signature over the payload
    cx_hash_sha256(workBuffer, payload_size, hash, sizeof(hash));

    error = check_signature_with_pubkey("External Plugin",
                                        hash,
                                        sizeof(hash),
                                        LEDGER_SIGNATURE_PUBLIC_KEY,
                                        sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
#ifdef HAVE_LEDGER_PKI
                                        CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
#endif
                                        (uint8_t *) (workBuffer + payload_size),
                                        dataLength - payload_size);
    if (error != CX_OK) {
        PRINTF("Invalid signature\n");
#ifndef HAVE_BYPASS_SIGNATURES
        THROW(APDU_RESPONSE_INVALID_DATA);
#endif
    }

    // move on to the rest of the payload parsing
    workBuffer++;
    memmove(dataContext.tokenContext.pluginName, workBuffer, pluginNameLength);
    dataContext.tokenContext.pluginName[pluginNameLength] = '\0';
    workBuffer += pluginNameLength;

    PRINTF("Check external plugin %s\n", dataContext.tokenContext.pluginName);

    // Check if the plugin is present on the device
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
            THROW(APDU_RESPONSE_PLUGIN_NOT_INSTALLED);
        }
        FINALLY {
        }
    }
    END_TRY;

    PRINTF("Plugin found\n");

    memmove(dataContext.tokenContext.contractAddress, workBuffer, ADDRESS_LENGTH);
    workBuffer += ADDRESS_LENGTH;
    memmove(dataContext.tokenContext.methodSelector, workBuffer, SELECTOR_SIZE);

    pluginType = EXTERNAL;

    U2BE_ENCODE(G_io_apdu_buffer, *tx, APDU_RESPONSE_OK);
    *tx += 2;
}
