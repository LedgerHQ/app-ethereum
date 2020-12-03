#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_flow.h"

void handleSetExternalPlugin(uint8_t p1,
                             uint8_t p2,
                             uint8_t *workBuffer,
                             uint16_t dataLength,
                             unsigned int *flags,
                             unsigned int *tx) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(flags);
    uint8_t pluginNameLength = dataLength;

    if (dataLength < 1) {
        THROW(0x6A80);
    }

    if (pluginNameLength + 1 > sizeof(dataContext.tokenContext.pluginName)) {
        THROW(0x6A80);
    }

    memmove(dataContext.tokenContext.pluginName, workBuffer, pluginNameLength);
    dataContext.tokenContext.pluginName[pluginNameLength] = '\0';

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
            THROW(0x6984);
        }
        FINALLY {
        }
    }
    END_TRY;

    tmpCtx.transactionContext.externalPluginIsSet = true;
    PRINTF("Plugin found\n");

    G_io_apdu_buffer[(*tx)++] = 0x90;
    G_io_apdu_buffer[(*tx)++] = 0x00;
}