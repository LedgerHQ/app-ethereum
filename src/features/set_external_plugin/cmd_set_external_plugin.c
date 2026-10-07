#include "apdu_constants.h"
#include "public_keys.h"
#include "eth_plugin_interface.h"
#include "eth_plugin_internal.h"
#include "plugin_utils.h"
#include "os_pki.h"
#include "os_lib.h"

uint16_t handle_set_external_plugin(const uint8_t *workBuffer, uint8_t dataLength) {
    PRINTF("Handling set Plugin\n");
    if (appState != APP_STATE_IDLE) {
        return SWO_COMMAND_NOT_ALLOWED;
    }
    // An Lc=0 APDU reaches here with a NULL data pointer; reject before deref.
    if (workBuffer == NULL || dataLength == 0) {
        return SWO_INCORRECT_DATA;
    }
    uint8_t hash[INT256_LENGTH];
    uint8_t pluginNameLength = *workBuffer;
    uint32_t params[2];

    PRINTF("plugin Name Length: %d\n", pluginNameLength);
    // Reject empty plugin names locally. The payload is also signed by Ledger
    // PKI and a backend should never sign an empty name, but enforcing the
    // bound here keeps the device side self-consistent and avoids depending
    // on a backend invariant we can't observe.
    if (pluginNameLength == 0) {
        PRINTF("empty plugin name\n");
        return SWO_INCORRECT_DATA;
    }
    const size_t payload_size = 1 + pluginNameLength + ADDRESS_LENGTH + SELECTOR_SIZE;

    if (dataLength <= payload_size) {
        PRINTF("data too small: expected at least %d got %d\n", payload_size, dataLength);
        return SWO_INCORRECT_DATA;
    }

    if ((size_t) pluginNameLength + 1 > sizeof(dataContext.tokenContext.pluginName)) {
        PRINTF("name length too big: expected max %d, got %d\n",
               sizeof(dataContext.tokenContext.pluginName),
               pluginNameLength + 1);
        return SWO_INCORRECT_DATA;
    }

    // check Ledger's signature over the payload
    cx_hash_sha256(workBuffer, payload_size, hash, sizeof(hash));

    if (check_signature_with_pubkey(hash,
                                    sizeof(hash),
                                    LEDGER_SIGNATURE_PUBLIC_KEY,
                                    sizeof(LEDGER_SIGNATURE_PUBLIC_KEY),
                                    CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META,
                                    (uint8_t *) (workBuffer + payload_size),
                                    dataLength - payload_size) != true) {
        return SWO_INCORRECT_DATA;
    }

    // move on to the rest of the payload parsing
    workBuffer++;
    memmove(dataContext.tokenContext.pluginName, workBuffer, pluginNameLength);
    dataContext.tokenContext.pluginName[pluginNameLength] = '\0';
    workBuffer += pluginNameLength;

    PRINTF("Check external plugin %s\n", dataContext.tokenContext.pluginName);

    // Check if the plugin is present on the device. Bridge through
    // uintptr_t so the cast is well-defined on test builds where
    // void* is wider than uint32_t (production target is 32-bit
    // ARM where the two widths match).
    params[0] = (uint32_t) (uintptr_t) dataContext.tokenContext.pluginName;
    params[1] = ETH_PLUGIN_CHECK_PRESENCE;
    BEGIN_TRY {
        TRY {
            os_lib_call(params);
        }
        CATCH_OTHER(e) {
            (void) e;
            PRINTF("%s external plugin is not present\n", dataContext.tokenContext.pluginName);
            memset(dataContext.tokenContext.pluginName,
                   0,
                   sizeof(dataContext.tokenContext.pluginName));
            CLOSE_TRY;
            return SWO_FILE_NOT_FOUND;
        }
        FINALLY {
        }
    }
    END_TRY;

    PRINTF("Plugin found\n");
    memmove(dataContext.tokenContext.contractAddress, workBuffer, ADDRESS_LENGTH);
    workBuffer += ADDRESS_LENGTH;
    memmove(dataContext.tokenContext.methodSelector, workBuffer, SELECTOR_SIZE);
    pluginType = PLUGIN_TYPE_EXTERNAL;
    // External-plugin registration is intentionally chain-unbound: the signed
    // payload here is [name_len | name | address | selector] and contains no
    // chain_id field, unlike handle_set_plugin which does carry one. Marking
    // the binding as PLUGIN_CHAIN_ID_ANY is required so the deferred chain
    // check in finalize_parsing_helper() lets the registration through.
    //
    // Closing the cross-chain replay surface for external plugins (Cerberus
    // CWE-345 follow-up) requires extending the signed payload format on
    // both the device and the metadata-signing side, and is intentionally
    // out of scope of this fix. Tracked separately, do not patch in place:
    // naively widening payload_size here would either reject every existing
    // signature (the extra bytes are not yet sent) or accept attacker-
    // controlled bytes outside the signed range.
    dataContext.tokenContext.pluginChainId = PLUGIN_CHAIN_ID_ANY;

    return SWO_SUCCESS;
}
