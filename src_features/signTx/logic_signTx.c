#include "shared_context.h"
#include "utils.h"
#include "ui_callbacks.h"
#include "ui_flow.h"
#include "feature_signTx.h"
#ifdef HAVE_STARKWARE
#include "stark_utils.h"
#endif
#include "eth_plugin_handler.h"
#include "network.h"

#define ERR_SILENT_MODE_CHECK_FAILED 0x6001

uint32_t splitBinaryParameterPart(char *result, uint8_t *parameter) {
    uint32_t i;
    for (i = 0; i < 8; i++) {
        if (parameter[i] != 0x00) {
            break;
        }
    }
    if (i == 8) {
        result[0] = '0';
        result[1] = '0';
        result[2] = '\0';
        return 2;
    } else {
        array_hexstr(result, parameter + i, 8 - i);
        return ((8 - i) * 2);
    }
}

customStatus_e customProcessor(txContext_t *context) {
    if (((context->txType == LEGACY && context->currentField == LEGACY_RLP_DATA) ||
         (context->txType == EIP2930 && context->currentField == EIP2930_RLP_DATA)) &&
        (context->currentFieldLength != 0)) {
        dataPresent = true;
        // If handling a new contract rather than a function call, abort immediately
        if (tmpContent.txContent.destinationLength == 0) {
            return CUSTOM_NOT_HANDLED;
        }
        // If data field is less than 4 bytes long, do not try to use a plugin.
        if (context->currentFieldLength < 4) {
            return CUSTOM_NOT_HANDLED;
        }
        if (context->currentFieldPos == 0) {
            ethPluginInitContract_t pluginInit;
            // If handling the beginning of the data field, assume that the function selector is
            // present
            if (context->commandLength < 4) {
                PRINTF("Missing function selector\n");
                return CUSTOM_FAULT;
            }
            dataContext.tokenContext.pluginStatus = ETH_PLUGIN_RESULT_UNAVAILABLE;
            // If contract debugging mode is activated, do not go through the plugin activation
            // as they wouldn't be displayed if the plugin consumes all data but fallbacks
            if (!N_storage.contractDetails) {
                eth_plugin_prepare_init(&pluginInit,
                                        context->workBuffer,
                                        context->currentFieldLength);
                dataContext.tokenContext.pluginStatus =
                    eth_plugin_perform_init(tmpContent.txContent.destination, &pluginInit);
            }
            PRINTF("pluginstatus %d\n", dataContext.tokenContext.pluginStatus);
            eth_plugin_result_t status = dataContext.tokenContext.pluginStatus;
            if (status == ETH_PLUGIN_RESULT_ERROR) {
                return CUSTOM_FAULT;
            } else if (status >= ETH_PLUGIN_RESULT_SUCCESSFUL) {
                dataContext.tokenContext.fieldIndex = 0;
                dataContext.tokenContext.fieldOffset = 0;
                copyTxData(context, NULL, 4);
                if (context->currentFieldLength == 4) {
                    return CUSTOM_NOT_HANDLED;
                }
            }
        }
        uint32_t blockSize;
        uint32_t copySize;
        uint32_t fieldPos = context->currentFieldPos;
        if (fieldPos == 0) {  // not reached if a plugin is available
            if (!N_storage.dataAllowed) {
                PRINTF("Data field forbidden\n");
                return CUSTOM_FAULT;
            }
            if (!N_storage.contractDetails) {
                return CUSTOM_NOT_HANDLED;
            }
            dataContext.tokenContext.fieldIndex = 0;
            dataContext.tokenContext.fieldOffset = 0;
            blockSize = 4;
        } else {
            if (!N_storage.contractDetails &&
                dataContext.tokenContext.pluginStatus <= ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
                return CUSTOM_NOT_HANDLED;
            }
            blockSize = 32 - (dataContext.tokenContext.fieldOffset % 32);
        }

        // Sanity check
        if ((context->currentFieldLength - fieldPos) < blockSize) {
            PRINTF("Unconsistent data\n");
            return CUSTOM_FAULT;
        }

        copySize = (context->commandLength < blockSize ? context->commandLength : blockSize);

        PRINTF("currentFieldPos %d copySize %d\n", context->currentFieldPos, copySize);

        copyTxData(context,
                   dataContext.tokenContext.data + dataContext.tokenContext.fieldOffset,
                   copySize);

        if (context->currentFieldPos == context->currentFieldLength) {
            context->currentField++;
            context->processingField = false;
        }

        dataContext.tokenContext.fieldOffset += copySize;

        if (copySize == blockSize) {
            // Can process or display
            if (dataContext.tokenContext.pluginStatus >= ETH_PLUGIN_RESULT_SUCCESSFUL) {
                ethPluginProvideParameter_t pluginProvideParameter;
                eth_plugin_prepare_provide_parameter(&pluginProvideParameter,
                                                     dataContext.tokenContext.data,
                                                     dataContext.tokenContext.fieldIndex * 32 + 4);
                if (!eth_plugin_call(ETH_PLUGIN_PROVIDE_PARAMETER,
                                     (void *) &pluginProvideParameter)) {
                    PRINTF("Plugin parameter call failed\n");
                    return CUSTOM_FAULT;
                }
                dataContext.tokenContext.fieldIndex++;
                dataContext.tokenContext.fieldOffset = 0;
                return CUSTOM_HANDLED;
            }

            if (fieldPos != 0) {
                dataContext.tokenContext.fieldIndex++;
            }
            dataContext.tokenContext.fieldOffset = 0;
            if (fieldPos == 0) {
                array_hexstr(strings.tmp.tmp, dataContext.tokenContext.data, 4);
                ux_flow_init(0, ux_confirm_selector_flow, NULL);
            } else {
                uint32_t offset = 0;
                uint32_t i;
                snprintf(strings.tmp.tmp2,
                         sizeof(strings.tmp.tmp2),
                         "Field %d",
                         dataContext.tokenContext.fieldIndex);
                for (i = 0; i < 4; i++) {
                    offset += splitBinaryParameterPart(strings.tmp.tmp + offset,
                                                       dataContext.tokenContext.data + 8 * i);
                    if (i != 3) {
                        strings.tmp.tmp[offset++] = ':';
                    }
                }
                ux_flow_init(0, ux_confirm_parameter_flow, NULL);
            }
        } else {
            return CUSTOM_HANDLED;
        }

        return CUSTOM_SUSPENDED;
    }
    return CUSTOM_NOT_HANDLED;
}

void to_uppercase(char *str, unsigned char size) {
    for (unsigned char i = 0; i < size && str[i] != 0; i++) {
        str[i] = str[i] >= 'a' ? str[i] - ('a' - 'A') : str[i];
    }
}

void compareOrCopy(char *preapproved_string, size_t size, char *parsed_string, bool silent_mode) {
    if (silent_mode) {
        /* ETH address are not fundamentally case sensitive but might
        have some for checksum purpose, so let's get rid of these diffs */
        to_uppercase(preapproved_string, strlen(preapproved_string));
        to_uppercase(parsed_string, strlen(parsed_string));
        if (memcmp(preapproved_string, parsed_string, strlen(preapproved_string))) {
            THROW(ERR_SILENT_MODE_CHECK_FAILED);
        }
    } else {
        strlcpy(preapproved_string, parsed_string, size);
    }
}

void reportFinalizeError(bool direct) {
    reset_app_context();
    if (direct) {
        THROW(0x6A80);
    } else {
        io_seproxyhal_send_status(0x6A80);
        ui_idle();
    }
}

void computeFees(char *displayBuffer, uint32_t displayBufferSize) {
    uint256_t gasPrice, startGas, uint256;
    char *feeTicker = get_network_ticker();
    uint8_t tickerOffset = 0;
    uint32_t i;

    PRINTF("Max fee\n");
    PRINTF("Gasprice %.*H\n",
           tmpContent.txContent.gasprice.length,
           tmpContent.txContent.gasprice.value);
    PRINTF("Startgas %.*H\n",
           tmpContent.txContent.startgas.length,
           tmpContent.txContent.startgas.value);
    convertUint256BE(tmpContent.txContent.gasprice.value,
                     tmpContent.txContent.gasprice.length,
                     &gasPrice);
    convertUint256BE(tmpContent.txContent.startgas.value,
                     tmpContent.txContent.startgas.length,
                     &startGas);
    mul256(&gasPrice, &startGas, &uint256);
    tostring256(&uint256, 10, (char *) (G_io_apdu_buffer + 100), 100);
    i = 0;
    while (G_io_apdu_buffer[100 + i]) {
        i++;
    }
    adjustDecimals((char *) (G_io_apdu_buffer + 100),
                   i,
                   (char *) G_io_apdu_buffer,
                   100,
                   WEI_TO_ETHER);
    i = 0;
    tickerOffset = 0;
    memset(displayBuffer, 0, displayBufferSize);
    while (feeTicker[tickerOffset]) {
        displayBuffer[tickerOffset] = feeTicker[tickerOffset];
        tickerOffset++;
    }
    while (G_io_apdu_buffer[i]) {
        displayBuffer[tickerOffset + i] = G_io_apdu_buffer[i];
        i++;
    }
    displayBuffer[tickerOffset + i] = '\0';
}

static void get_public_key(uint8_t *out, uint8_t outLength) {
    uint8_t privateKeyData[INT256_LENGTH] = {0};
    cx_ecfp_private_key_t privateKey = {0};
    cx_ecfp_public_key_t publicKey = {0};

    if (outLength < ADDRESS_LENGTH) {
        return;
    }

    os_perso_derive_node_bip32(CX_CURVE_256K1,
                               tmpCtx.transactionContext.bip32Path,
                               tmpCtx.transactionContext.pathLength,
                               privateKeyData,
                               NULL);
    cx_ecfp_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    cx_ecfp_generate_pair(CX_CURVE_256K1, &publicKey, &privateKey, 1);
    explicit_bzero(&privateKey, sizeof(privateKey));
    explicit_bzero(privateKeyData, sizeof(privateKeyData));
    getEthAddressFromKey(&publicKey, out, &global_sha3);
}

void finalizeParsing(bool direct) {
    char displayBuffer[50];
    uint8_t decimals = WEI_TO_ETHER;
    char *ticker = get_network_ticker();
    ethPluginFinalize_t pluginFinalize;
    bool genericUI = true;

    // Verify the chain
    if (chainConfig->chainId != ETHEREUM_MAINNET_CHAINID) {
        uint32_t id = get_chain_id();

        if (chainConfig->chainId != id) {
            PRINTF("Invalid chainID %u expected %u\n", id, chainConfig->chainId);
            reset_app_context();
            reportFinalizeError(direct);
            if (!direct) {
                return;
            }
        }
    }
    // Store the hash
    cx_hash((cx_hash_t *) &global_sha3,
            CX_LAST,
            tmpCtx.transactionContext.hash,
            0,
            tmpCtx.transactionContext.hash,
            32);

    // Finalize the plugin handling
    if (dataContext.tokenContext.pluginStatus >= ETH_PLUGIN_RESULT_SUCCESSFUL) {
        genericUI = false;
        eth_plugin_prepare_finalize(&pluginFinalize);

        uint8_t msg_sender[ADDRESS_LENGTH] = {0};
        get_public_key(msg_sender, sizeof(msg_sender));
        pluginFinalize.address = msg_sender;

        if (!eth_plugin_call(ETH_PLUGIN_FINALIZE, (void *) &pluginFinalize)) {
            PRINTF("Plugin finalize call failed\n");
            reportFinalizeError(direct);
            if (!direct) {
                return;
            }
        }
        // Lookup tokens if requested
        ethPluginProvideToken_t pluginProvideToken;
        eth_plugin_prepare_provide_token(&pluginProvideToken);
        if ((pluginFinalize.tokenLookup1 != NULL) || (pluginFinalize.tokenLookup2 != NULL)) {
            if (pluginFinalize.tokenLookup1 != NULL) {
                PRINTF("Lookup1: %.*H\n", ADDRESS_LENGTH, pluginFinalize.tokenLookup1);
                pluginProvideToken.token1 = getKnownToken(pluginFinalize.tokenLookup1);
                if (pluginProvideToken.token1 != NULL) {
                    PRINTF("Token1 ticker: %s\n", pluginProvideToken.token1->ticker);
                }
            }
            if (pluginFinalize.tokenLookup2 != NULL) {
                PRINTF("Lookup2: %.*H\n", ADDRESS_LENGTH, pluginFinalize.tokenLookup2);
                pluginProvideToken.token2 = getKnownToken(pluginFinalize.tokenLookup2);
                if (pluginProvideToken.token2 != NULL) {
                    PRINTF("Token2 ticker: %s\n", pluginProvideToken.token2->ticker);
                }
            }
            if (eth_plugin_call(ETH_PLUGIN_PROVIDE_TOKEN, (void *) &pluginProvideToken) <=
                ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
                PRINTF("Plugin provide token call failed\n");
                reportFinalizeError(direct);
                if (!direct) {
                    return;
                }
            }
            pluginFinalize.result = pluginProvideToken.result;
        }
        if (pluginFinalize.result != ETH_PLUGIN_RESULT_FALLBACK) {
            // Handle the right interface
            switch (pluginFinalize.uiType) {
                case ETH_UI_TYPE_GENERIC:
                    dataPresent = false;
                    // Add the number of screens + the number of additional screens to get the total
                    // number of screens needed.
                    dataContext.tokenContext.pluginUiMaxItems =
                        pluginFinalize.numScreens + pluginProvideToken.additionalScreens;
                    break;
                case ETH_UI_TYPE_AMOUNT_ADDRESS:
                    genericUI = true;
                    dataPresent = false;
                    if ((pluginFinalize.amount == NULL) || (pluginFinalize.address == NULL)) {
                        PRINTF("Incorrect amount/address set by plugin\n");
                        reportFinalizeError(direct);
                        if (!direct) {
                            return;
                        }
                    }
                    memmove(tmpContent.txContent.value.value, pluginFinalize.amount, 32);
                    tmpContent.txContent.value.length = 32;
                    memmove(tmpContent.txContent.destination, pluginFinalize.address, 20);
                    tmpContent.txContent.destinationLength = 20;
                    if (pluginProvideToken.token1 != NULL) {
                        decimals = pluginProvideToken.token1->decimals;
                        ticker = pluginProvideToken.token1->ticker;
                    }
                    break;
                default:
                    PRINTF("ui type %d not supported\n", pluginFinalize.uiType);
                    reportFinalizeError(direct);
                    if (!direct) {
                        return;
                    }
            }
        } else {
            genericUI = true;
        }
    }

    if (dataPresent && !N_storage.dataAllowed) {
        reportFinalizeError(direct);
        if (!direct) {
            return;
        }
    }
    // Prepare destination address to display
    if (genericUI) {
        if (tmpContent.txContent.destinationLength != 0) {
            displayBuffer[0] = '0';
            displayBuffer[1] = 'x';
            getEthAddressStringFromBinary(tmpContent.txContent.destination,
                                          displayBuffer + 2,
                                          &global_sha3,
                                          chainConfig);
            compareOrCopy(strings.common.fullAddress,
                          sizeof(strings.common.fullAddress),
                          displayBuffer,
                          called_from_swap);
        } else {
            strcpy(strings.common.fullAddress, "Contract");
        }
    }
    // Prepare amount to display
    if (genericUI) {
        amountToString(tmpContent.txContent.value.value,
                       tmpContent.txContent.value.length,
                       decimals,
                       ticker,
                       displayBuffer,
                       sizeof(displayBuffer));
        compareOrCopy(strings.common.fullAmount,
                      sizeof(strings.common.fullAddress),
                      displayBuffer,
                      called_from_swap);
    }
    // Prepare nonce to display
    if (genericUI) {
        uint256_t nonce;
        convertUint256BE(tmpContent.txContent.nonce.value,
                         tmpContent.txContent.nonce.length,
                         &nonce);
        tostring256(&nonce, 10, displayBuffer, sizeof(displayBuffer));
        strncpy(strings.common.nonce, displayBuffer, sizeof(strings.common.nonce));
    }
    // Compute maximum fee
    if (genericUI) {
        computeFees(displayBuffer, sizeof(displayBuffer));
        compareOrCopy(strings.common.maxFee,
                      sizeof(strings.common.maxFee),
                      displayBuffer,
                      called_from_swap);
    }

    // Prepare chainID field
    if (genericUI) {
        char *name = get_network_name();
        if (name == NULL) {
            // No network name found so simply copy the chain ID as the network name.
            uint32_t chain_id = get_chain_id();
            uint8_t res = snprintf(strings.common.network_name,
                                   sizeof(strings.common.network_name),
                                   "%d",
                                   chain_id);
            if (res >= sizeof(strings.common.network_name)) {
                // If the return value is higher or equal to the size passed in as parameter, then
                // the output was truncated. Return the appropriate error code.
                THROW(0x6502);
            }
        } else {
            // Network name found, simply copy it.
            strncpy(strings.common.network_name, name, sizeof(strings.common.network_name));
        }
    }

    bool no_consent;

    no_consent = called_from_swap;

#ifdef NO_CONSENT
    no_consent = true;
#endif  // NO_CONSENT

    if (no_consent) {
        io_seproxyhal_touch_tx_ok(NULL);
    } else {
        if (genericUI) {
            ux_approve_tx(dataPresent);
        } else {
            plugin_ui_start();
        }
    }
}
