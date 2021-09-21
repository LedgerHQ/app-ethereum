#include "erc721_plugin.h"
#include "eth_plugin_internal.h"

static const uint8_t ERC721_APPROVE_SELECTOR[SELECTOR_SIZE] = {0x13, 0x37, 0x42, 0x42};
static const uint8_t ERC721_APPROVE_FOR_ALL_SELECTOR[SELECTOR_SIZE] = {0xa2, 0x2c, 0xb4, 0x65};
static const uint8_t ERC721_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0x23, 0xb8, 0x72, 0xdd};
static const uint8_t ERC721_SAFE_TRANSFER_SELECTOR[SELECTOR_SIZE] = {0x42, 0x84, 0x2e, 0x0e};
// SCOTT TODO: missing SAFE_TRANFSFER_DATA

const uint8_t *const ERC721_SELECTORS[NUM_ERC721_SELECTORS] = {ERC721_APPROVE_SELECTOR,
                                                               ERC721_APPROVE_FOR_ALL_SELECTOR,
                                                               ERC721_TRANSFER_SELECTOR,
                                                               ERC721_SAFE_TRANSFER_SELECTOR};

static void handle_init_contract(void *parameters) {
    ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    uint8_t i;
    for (i = 0; i < NUM_ERC721_SELECTORS; i++) {
        if (memcmp((uint8_t *) PIC(ERC721_SELECTORS[i]), msg->selector, SELECTOR_SIZE) == 0) {
            context->selectorIndex = i;
            break;
        }
    }

    // No selector found.
    if (i == NUM_ERC721_SELECTORS) {
        PRINTF("Unknown erc721 selector %.*H\n", SELECTOR_SIZE, msg->selector);
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }

    msg->result = ETH_PLUGIN_RESULT_OK;
    switch (context->selectorIndex) {
        case SET_APPROVAL_FOR_ALL:
        case APPROVE:
            context->next_param = OPERATOR;
            break;
        case SAFE_TRANSFER:
        case SAFE_TRANSFER_DATA:
        case TRANSFER:
            context->next_param = FROM;
            break;
        default:
            PRINTF("Unsupported selector index: %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

static void handle_finalize(void *parameters) {
    ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
    msg->tokenLookup2 = context->address;
    msg->numScreens = 3;
    msg->uiType = ETH_UI_TYPE_GENERIC;
    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_provide_token(void *parameters) {
    ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;

    msg->result = ETH_PLUGIN_RESULT_OK;
}

static void handle_query_contract_id(void *parameters) {
    ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
    erc721_parameters_t *context = (erc721_parameters_t *) msg->pluginContext;

    msg->result = ETH_PLUGIN_RESULT_OK;

    strlcpy(msg->name, "NFT", msg->nameLength);

    switch (context->selectorIndex) {
        case SET_APPROVAL_FOR_ALL:
        case APPROVE:
            strlcpy(msg->version, "Allowance", msg->versionLength);
            break;
        case SAFE_TRANSFER:
        case SAFE_TRANSFER_DATA:
        case TRANSFER:
            strlcpy(msg->version, "Transfer", msg->versionLength);
            break;
        default:
            PRINTF("Unsupported selector %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            break;
    }
}

bool erc721_plugin_available_check() {
    return true;
}

void erc721_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            handle_init_contract(parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            handle_provide_parameter(parameters);
        } break;
        case ETH_PLUGIN_FINALIZE: {
            handle_finalize(parameters);
        } break;
        case ETH_PLUGIN_PROVIDE_TOKEN: {
            handle_provide_token(parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            handle_query_contract_id(parameters);
        } break;
        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            handle_query_contract_ui(parameters);
        } break;
        default:
            PRINTF("Unhandled message %d\n", message);
            break;
    }
}
