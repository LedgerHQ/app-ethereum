#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"

void starkware_print_stark_key(uint8_t *starkKey, char *destination);
void starkware_print_eth_address(uint8_t *address, char *destination);

typedef struct erc721_parameters_t {
    uint8_t selectorIndex;
    uint8_t address[ADDRESS_LENGTH];
    uint8_t tokenId[INT256_LENGTH];
    // tokenDefinition_t *tokenSelf;
    // tokenDefinition_t *tokenAddress;
} erc721_parameters_t;

bool erc721_plugin_available_check() {
#ifdef HAVE_STARKWARE
    if (quantumSet) {
        switch (dataContext.tokenContext.quantumType) {
            case STARK_QUANTUM_ERC721:
            case STARK_QUANTUM_MINTABLE_ERC721:
                return true;
            default:
                return false;
        }
    }
    return false;
#endif
}

void erc721_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;
            // enforce that ETH amount should be 0
            if (!allzeroes(msg->pluginSharedRO->txContent->value.value, 32)) {
                PRINTF("Err: Transaction amount is not 0 for erc721 approval\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            } else {
                size_t i;
                for (i = 0; i < NUM_ERC721_SELECTORS; i++) {
                    if (memcmp((uint8_t *) PIC(ERC721_SELECTORS[i]),
                               msg->selector,
                               SELECTOR_SIZE) == 0) {
                        context->selectorIndex = i;
                        break;
                    }
                }
                if (i == NUM_ERC721_SELECTORS) {
                    PRINTF("Unknown erc721 selector %.*H\n", SELECTOR_SIZE, msg->selector);
                    break;
                }
                if (msg->dataSize != 4 + 32 + 32) {
                    PRINTF("Invalid erc721 approval data size %d\n", msg->dataSize);
                    break;
                }
                PRINTF("erc721 plugin init\n");
                msg->result = ETH_PLUGIN_RESULT_OK;
            }
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;
            PRINTF("erc721 plugin provide parameter %d %.*H\n",
                   msg->parameterOffset,
                   32,
                   msg->parameter);
            switch (msg->parameterOffset) {
                case 4:
                    memmove(context->address, msg->parameter + 32 - 20, 20);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                case 4 + 32:
                    memmove(context->tokenId, msg->parameter, 32);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;
                default:
                    PRINTF("Unhandled parameter offset\n");
                    break;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;
            PRINTF("erc721 plugin finalize\n");
            msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
            msg->tokenLookup2 = context->address;
            msg->numScreens = 3;
            msg->uiType = ETH_UI_TYPE_GENERIC;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_TOKEN: {
            ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;
            PRINTF("erc721 plugin provide token dest: %d - address: %d\n",
                   (msg->token1 != NULL),
                   (msg->token2 != NULL));
            // context->tokenSelf = msg->token1;
            // context->tokenAddress = msg->token2;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            strlcpy(msg->name, "Allowance", msg->nameLength);
            strlcpy(msg->version, "", msg->versionLength);
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0:
                    strlcpy(msg->title, "Contract Name", msg->titleLength);
                    starkware_print_eth_address(tmpContent.txContent.destination, msg->msg);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 1:
                    strlcpy(msg->title, "NFT Contract", msg->titleLength);
                    starkware_print_eth_address(context->address, msg->msg);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 2:
                    strlcpy(msg->title, "TokenID", msg->titleLength);
                    starkware_print_stark_key(context->tokenId, msg->msg);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                default:
                    break;
            }
        } break;

        default:
            PRINTF("Unhandled message %d\n", message);
    }
}
