#include <string.h>
#include "eth_plugin_interface.h"
#include "shared_context.h"       // TODO : rewrite as independant code
#include "eth_plugin_internal.h"  // TODO : rewrite as independant code
#include "utils.h"

typedef enum {
    COMPOUND_REDEEM_UNDERLYING = 0,
    COMPOUND_REDEEM,
    COMPOUND_MINT,
    CETH_MINT
} compoundSelector_t;

static const uint8_t COMPOUND_EXPECTED_DATA_SIZE[] = {
    4 + 32,
    4 + 32,
    4 + 32,
    4,
};

// redeemUnderlying : redeemAmount (32)
// redeem underlying token
// redeem : redeemTokens (32)
// redeem Ctoken
// mint : mintAmount (32)
// lend some token
// mint :
// lend some Ether

typedef struct compound_parameters_t {
    uint8_t selectorIndex;
    uint8_t amount[32];
    char ticker_1[MAX_TICKER_LEN];
    uint8_t decimals;
} compound_parameters_t;

typedef struct underlying_asset_decimals_t {
    char c_ticker[MAX_TICKER_LEN];
    uint8_t decimals;
} underlying_asset_decimals_t;

/* Sadly, we don't have the information about the underlying asset's decimals, which can differ from
the cToken decimals. Therefore, we hardcode a binding table. If Compound adds a lot of token in the
future, we will have to move to a CAL based architecture instead, as this one doesn't scale well.*/
#define NUM_COMPOUND_BINDINGS 9
const underlying_asset_decimals_t UNDERLYING_ASSET_DECIMALS[NUM_COMPOUND_BINDINGS] = {
    {"cDAI", 18},
    {"CETH", 18},
    {"CUSDC", 6},
    {"CZRX", 18},
    {"CUSDT", 6},
    {"CBTC", 8},
    {"CBAT", 18},
    {"CREP", 18},
    {"cSAI", 18},
};

bool get_underlying_asset_decimals(char *compound_ticker, uint8_t *out_decimals) {
    for (size_t i = 0; i < NUM_COMPOUND_BINDINGS; i++) {
        underlying_asset_decimals_t *binding =
            (underlying_asset_decimals_t *) PIC(&UNDERLYING_ASSET_DECIMALS[i]);
        if (strncmp(binding->c_ticker,
                    compound_ticker,
                    strnlen(binding->c_ticker, MAX_TICKER_LEN)) == 0) {
            *out_decimals = binding->decimals;
            return true;
        }
    }
    return false;
}

void compound_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            compound_parameters_t *context = (compound_parameters_t *) msg->pluginContext;
            size_t i;
            for (i = 0; i < NUM_COMPOUND_SELECTORS; i++) {
                if (memcmp((uint8_t *) PIC(COMPOUND_SELECTORS[i]), msg->selector, SELECTOR_SIZE) ==
                    0) {
                    context->selectorIndex = i;
                    break;
                }
            }
            // enforce that ETH amount should be 0, except in ceth.mint case
            if (!allzeroes(msg->pluginSharedRO->txContent->value.value, 32)) {
                if (context->selectorIndex != CETH_MINT) {
                    msg->result = ETH_PLUGIN_RESULT_ERROR;
                    break;
                }
            }
            if (i == NUM_COMPOUND_SELECTORS) {
                PRINTF("Unknown selector %.*H\n", SELECTOR_SIZE, msg->selector);
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (msg->dataSize != COMPOUND_EXPECTED_DATA_SIZE[context->selectorIndex]) {
                PRINTF("Unexpected data size for command %d expected %d got %d\n",
                       context->selectorIndex,
                       COMPOUND_EXPECTED_DATA_SIZE[context->selectorIndex],
                       msg->dataSize);
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                break;
            }
            if (context->selectorIndex == CETH_MINT) {
                // ETH amount 0x1234 is stored 0x12340000...000 instead of 0x00....001234, so we
                // strip the following zeroes when copying
                memset(context->amount, 0, sizeof(context->amount));
                memmove(context->amount + sizeof(context->amount) -
                            msg->pluginSharedRO->txContent->value.length,
                        msg->pluginSharedRO->txContent->value.value,
                        msg->pluginSharedRO->txContent->value.length);
            }
            PRINTF("compound plugin inititialized\n");
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            compound_parameters_t *context = (compound_parameters_t *) msg->pluginContext;
            PRINTF("compound plugin provide parameter %d %.*H\n",
                   msg->parameterOffset,
                   32,
                   msg->parameter);
            if (context->selectorIndex != CETH_MINT) {
                switch (msg->parameterOffset) {
                    case 4:
                        memmove(context->amount, msg->parameter, 32);
                        msg->result = ETH_PLUGIN_RESULT_OK;
                        break;
                    default:
                        PRINTF("Unhandled parameter offset\n");
                        msg->result = ETH_PLUGIN_RESULT_ERROR;
                        break;
                }
            } else {
                PRINTF("CETH contract expects no parameters\n");
                msg->result = ETH_PLUGIN_RESULT_ERROR;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            PRINTF("compound plugin finalize\n");
            msg->tokenLookup1 = msg->pluginSharedRO->txContent->destination;
            msg->numScreens = 2;
            msg->uiType = ETH_UI_TYPE_GENERIC;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_TOKEN: {
            ethPluginProvideToken_t *msg = (ethPluginProvideToken_t *) parameters;
            compound_parameters_t *context = (compound_parameters_t *) msg->pluginContext;
            PRINTF("compound plugin provide token: %d\n", (msg->token1 != NULL));
            if (msg->token1 != NULL) {
                strlcpy(context->ticker_1, msg->token1->ticker, MAX_TICKER_LEN);
                switch (context->selectorIndex) {
                    case COMPOUND_REDEEM_UNDERLYING:
                    case COMPOUND_MINT:
                    case CETH_MINT:
                        msg->result =
                            get_underlying_asset_decimals(context->ticker_1, &context->decimals)
                                ? ETH_PLUGIN_RESULT_OK
                                : ETH_PLUGIN_RESULT_FALLBACK;
                        break;

                    // Only case where we use the compound contract decimals
                    case COMPOUND_REDEEM:
                        context->decimals = msg->token1->decimals;
                        msg->result = ETH_PLUGIN_RESULT_OK;
                        break;

                    default:
                        msg->result = ETH_PLUGIN_RESULT_FALLBACK;
                        break;
                }
            } else {
                msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            }
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            compound_parameters_t *context = (compound_parameters_t *) msg->pluginContext;
            strlcpy(msg->name, "Type", msg->nameLength);
            switch (context->selectorIndex) {
                case COMPOUND_REDEEM_UNDERLYING:
                case COMPOUND_REDEEM:
                    strlcpy(msg->version, "Redeem", msg->versionLength);
                    break;

                case COMPOUND_MINT:
                case CETH_MINT:
                    strlcpy(msg->version, "Lend", msg->versionLength);
                    break;

                default:
                    break;
            }
            strlcat(msg->version, " Assets", msg->versionLength);
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            compound_parameters_t *context = (compound_parameters_t *) msg->pluginContext;
            switch (msg->screenIndex) {
                case 0: {
                    strlcpy(msg->title, "Amount", msg->titleLength);
                    char *ticker_ptr = context->ticker_1;
                    /* skip "c" in front of cToken unless we use "redeem", as
                    redeem is the only operation dealing with a cToken amount */
                    if (context->selectorIndex != COMPOUND_REDEEM) {
                        ticker_ptr++;
                    }
                    amountToString(context->amount,
                                   sizeof(context->amount),
                                   context->decimals,
                                   ticker_ptr,
                                   msg->msg,
                                   100);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;

                case 1:
                    strlcpy(msg->title, "Contract", msg->titleLength);
                    strlcpy(msg->msg, "Compound ", msg->msgLength);
                    strlcat(msg->msg,
                            context->ticker_1 + 1,
                            msg->msgLength);  // remove the 'c' char at beginning of compound ticker
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
