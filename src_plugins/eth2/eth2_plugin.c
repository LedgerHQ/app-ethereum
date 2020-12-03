#ifdef HAVE_ETH2

#include <string.h>
#include "eth_plugin_internal.h"
#include "eth_plugin_handler.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "utils.h"

void getEth2PublicKey(uint32_t *bip32Path, uint8_t bip32PathLength, uint8_t *out);

#define WITHDRAWAL_KEY_PATH_1 12381
#define WITHDRAWAL_KEY_PATH_2 3600
#define WITHDRAWAL_KEY_PATH_4 0

#define ETH2_DEPOSIT_PUBKEY_OFFSET         0x80
#define ETH2_WITHDRAWAL_CREDENTIALS_OFFSET 0xE0
#define ETH2_SIGNATURE_OFFSET              0x120
#define ETH2_DEPOSIT_PUBKEY_LENGTH         0x30
#define ETH2_WITHDRAWAL_CREDENTIALS_LENGTH 0x20
#define ETH2_SIGNATURE_LENGTH              0x60

typedef struct eth2_deposit_parameters_t {
    uint8_t valid;
} eth2_deposit_parameters_t;

void eth2_plugin_call(int message, void *parameters) {
    switch (message) {
        case ETH_PLUGIN_INIT_CONTRACT: {
            ethPluginInitContract_t *msg = (ethPluginInitContract_t *) parameters;
            eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t *) msg->pluginContext;
            context->valid = 1;
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_PROVIDE_PARAMETER: {
            ethPluginProvideParameter_t *msg = (ethPluginProvideParameter_t *) parameters;
            eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t *) msg->pluginContext;
            uint32_t index;
            PRINTF("eth2 plugin provide parameter %d %.*H\n",
                   msg->parameterOffset,
                   32,
                   msg->parameter);
            switch (msg->parameterOffset) {
                case 4 + (32 * 0):  // pubkey offset
                case 4 + (32 * 1):  // withdrawal credentials offset
                case 4 + (32 * 2):  // signature offset
                case 4 + (32 * 4):  // deposit pubkey length
                case 4 + (32 * 7):  // withdrawal credentials length
                case 4 + (32 * 9):  // signature length
                {
                    uint32_t check = 0;
                    switch (msg->parameterOffset) {
                        case 4 + (32 * 0):
                            check = ETH2_DEPOSIT_PUBKEY_OFFSET;
                            break;
                        case 4 + (32 * 1):
                            check = ETH2_WITHDRAWAL_CREDENTIALS_OFFSET;
                            break;
                        case 4 + (32 * 2):
                            check = ETH2_SIGNATURE_OFFSET;
                            break;
                        case 4 + (32 * 4):
                            check = ETH2_DEPOSIT_PUBKEY_LENGTH;
                            break;
                        case 4 + (32 * 7):
                            check = ETH2_WITHDRAWAL_CREDENTIALS_LENGTH;
                            break;
                        case 4 + (32 * 9):
                            check = ETH2_SIGNATURE_LENGTH;
                            break;
                        default:
                            break;
                    }
                    index = U4BE(msg->parameter, 32 - 4);
                    if (index != check) {
                        PRINTF("eth2 plugin parameter check %d failed, expected %d got %d\n",
                               msg->parameterOffset,
                               check,
                               index);
                        context->valid = 0;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;

                case 4 + (32 * 3):  // deposit data root
                case 4 + (32 * 5):  // deposit pubkey
                case 4 + (32 * 6):
                case 4 + (32 * 10):  // signature
                case 4 + (32 * 11):
                case 4 + (32 * 12):
                    msg->result = ETH_PLUGIN_RESULT_OK;
                    break;

                case 4 + (32 * 8):  // withdrawal credentials
                {
                    uint8_t tmp[48];
                    uint32_t withdrawalKeyPath[4];
                    withdrawalKeyPath[0] = WITHDRAWAL_KEY_PATH_1;
                    withdrawalKeyPath[1] = WITHDRAWAL_KEY_PATH_2;
                    withdrawalKeyPath[2] = eth2WithdrawalIndex;
                    withdrawalKeyPath[3] = WITHDRAWAL_KEY_PATH_4;
                    getEth2PublicKey(withdrawalKeyPath, 4, tmp);
                    PRINTF("eth2 plugin computed withdrawal public key %.*H\n", 48, tmp);
                    cx_hash_sha256(tmp, 48, tmp, 32);
                    tmp[0] = 0;
                    if (memcmp(tmp, msg->parameter, 32) != 0) {
                        PRINTF("eth2 plugin invalid withdrawal credentials\n");
                        PRINTF("Got %.*H\n", 32, msg->parameter);
                        PRINTF("Expected %.*H\n", 32, tmp);
                        context->valid = 0;
                    }
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;

                default:
                    PRINTF("Unhandled parameter offset\n");
                    break;
            }
        } break;

        case ETH_PLUGIN_FINALIZE: {
            ethPluginFinalize_t *msg = (ethPluginFinalize_t *) parameters;
            eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t *) msg->pluginContext;
            PRINTF("eth2 plugin finalize\n");
            if (context->valid) {
                msg->numScreens = 1;
                msg->uiType = ETH_UI_TYPE_GENERIC;
                msg->result = ETH_PLUGIN_RESULT_OK;
            } else {
                msg->result = ETH_PLUGIN_RESULT_FALLBACK;
            }
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_ID: {
            ethQueryContractID_t *msg = (ethQueryContractID_t *) parameters;
            strcpy(msg->name, "ETH2");
            strcpy(msg->version, "Deposit");
            msg->result = ETH_PLUGIN_RESULT_OK;
        } break;

        case ETH_PLUGIN_QUERY_CONTRACT_UI: {
            ethQueryContractUI_t *msg = (ethQueryContractUI_t *) parameters;
            // eth2_deposit_parameters_t *context = (eth2_deposit_parameters_t*)msg->pluginContext;
            switch (msg->screenIndex) {
                case 0: {
                    uint8_t decimals = WEI_TO_ETHER;
                    uint8_t *ticker = (uint8_t *) PIC(chainConfig->coinName);
                    strcpy(msg->title, "Amount");
                    amountToString(tmpContent.txContent.value.value,
                                   tmpContent.txContent.value.length,
                                   decimals,
                                   (char *) ticker,
                                   msg->msg,
                                   100);
                    msg->result = ETH_PLUGIN_RESULT_OK;
                } break;
                default:
                    break;
            }
        } break;

        default:
            PRINTF("Unhandled message %d\n", message);
    }
}

#endif
