#include "shared_context.h"
#include "utils.h"
#include "ui_callbacks.h"
#include "ui_flow.h"
#ifdef HAVE_STARKWARE
#include "stark_utils.h"
#endif
#include "eth_plugin_handler.h"

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
         (context->txType == EIP2930 && context->currentField == EIP2930_RLP_DATA) ||
         (context->txType == EIP1559 && context->currentField == EIP1559_RLP_DATA)) &&
        (context->currentFieldLength != 0)) {
        context->content->dataPresent = true;
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

void compareOrCopy(char *preapproved_string, char *parsed_string, bool silent_mode) {
    if (silent_mode) {
        /* ETH address are not fundamentally case sensitive but might
        have some for checksum purpose, so let's get rid of these diffs */
        to_uppercase(preapproved_string, strlen(preapproved_string));
        to_uppercase(parsed_string, strlen(parsed_string));
        if (memcmp(preapproved_string, parsed_string, strlen(preapproved_string))) {
            THROW(ERR_SILENT_MODE_CHECK_FAILED);
        }
    } else {
        strcpy(preapproved_string, parsed_string);
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

// Convert `BEgasPrice` and `BEgasLimit` to Uint256 and then stores the multiplication of both in `output`.
static void computeFees(txInt256_t *BEgasPrice, txInt256_t *BEgasLimit, uint256_t *output) {
    uint256_t gasPrice = {0};
    uint256_t gasLimit = {0};

    PRINTF("Gas price %.*H\n",
           BEgasPrice->length,
           BEgasPrice->value);
    PRINTF("Gas limit %.*H\n",
           BEgasLimit->length,
           BEgasLimit->value);
    convertUint256BE(BEgasPrice->value,
                     BEgasPrice->length,
                     &gasPrice);
    convertUint256BE(BEgasLimit->value,
                     BEgasLimit->length,
                     &gasLimit);
    mul256(&gasPrice, &gasLimit, output);
}

static void feesToString(uint256_t *rawFee, char *displayBuffer, uint32_t displayBufferSize) {
    uint8_t *feeTicker = (uint8_t *) PIC(chainConfig->coinName);
    uint8_t tickerOffset = 0;
    uint32_t i;

    tostring256(rawFee, 10, (char *) (G_io_apdu_buffer + 100), 100);
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
    PRINTF("Displayed fees: %s\n", displayBuffer);
}

// Compute the fees, transform it to a string, prepend a ticker to it and copy everything to `displayBuffer`.
void prepareAndCopyFees(txInt256_t *BEGasPrice, txInt256_t *BEGasLimit, char *displayBuffer, uint32_t displayBufferSize) {
    uint256_t rawFee = {0};
    computeFees(BEGasPrice, BEGasLimit, &rawFee);
    feesToString(&rawFee, displayBuffer, displayBufferSize);
}

static void prepareAndCopyDetailedFees() {
    uint256_t rawPriorityFee = {0};
    uint256_t rawMaxFee = {0};
    uint256_t rawBaseFee = {0};

    // Compute the priorityFee and the maxFee.
    computeFees(&tmpContent.txContent.maxPriorityFeePerGas, &tmpContent.txContent.startgas, &rawPriorityFee);
    computeFees(&tmpContent.txContent.gasprice, &tmpContent.txContent.startgas, &rawMaxFee);
    // Substract priorityFee from maxFee -> this is the baseFee
    minus256(&rawMaxFee, &rawPriorityFee, &rawBaseFee);

    // Transform priorityFee to string (with a ticker).
    PRINTF("Computing priority fee\n");
    feesToString(&rawPriorityFee, strings.common.priorityFee, sizeof(strings.common.priorityFee));

    PRINTF("Computing base fee\n");
    // Transform priorityFee to string (with a ticker).
    feesToString(&rawBaseFee, strings.common.maxFee, sizeof(strings.common.maxFee));
}

void prepareFeeDisplay() {
    if (N_storage.displayFeeDetails) {
        prepareAndCopyDetailedFees();
    } else {
        prepareAndCopyFees(&tmpContent.txContent.gasprice, &tmpContent.txContent.startgas, strings.common.maxFee, sizeof(strings.common.maxFee));
    }
}

uint32_t get_chainID() {
    uint32_t chain_id = 0;

    if (txContext.txType == LEGACY) {
        chain_id = u32_from_BE(txContext.content->v, txContext.content->vLength, true);
    } else if (txContext.txType == EIP2930 || txContext.txType == EIP1559) {
        chain_id = u32_from_BE(tmpContent.txContent.chainID.value, tmpContent.txContent.chainID.length, true);
    }
    else {
        PRINTF("Txtype `%u` not supported while generating chainID\n", txContext.txType);
    }
    PRINTF("ChainID: %d\n", chain_id);
    return chain_id;
}

void prepareChainIdDisplay() {
    uint32_t chainID = get_chainID();
    uint8_t res = snprintf(strings.common.chainID, sizeof(strings.common.chainID), "%d", chainID);
    if (res >= sizeof(strings.common.chainID)) {
        // If the return value is higher or equal to the size passed in as parameter, then
        // the output was truncated. Return the appropriate error code.
        THROW(0x6502);
    }
}

void finalizeParsing(bool direct) {
    char displayBuffer[50];
    uint8_t decimals = WEI_TO_ETHER;
    uint8_t *ticker = (uint8_t *) PIC(chainConfig->coinName);
    ethPluginFinalize_t pluginFinalize;
    tokenDefinition_t *token1 = NULL, *token2 = NULL;
    bool genericUI = true;

    // Verify the chain
    if (chainConfig->chainId != 0) {
        uint32_t id = 0;

        if (txContext.txType == LEGACY) {
            id = u32_from_BE(txContext.content->v, txContext.content->vLength, true);
        } else if (txContext.txType == EIP2930 || txContext.txType == EIP1559) {
            id = u32_from_BE(txContext.content->chainID.value,
                             txContext.content->chainID.length,
                             false);
        } else {
            PRINTF("TxType `%u` not supported while checking for chainID\n", txContext.txType);
            return;
        }

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
        if (!eth_plugin_call(ETH_PLUGIN_FINALIZE, (void *) &pluginFinalize)) {
            PRINTF("Plugin finalize call failed\n");
            reportFinalizeError(direct);
            if (!direct) {
                return;
            }
        }
        // Lookup tokens if requested
        ethPluginProvideToken_t pluginProvideToken;
        if ((pluginFinalize.tokenLookup1 != NULL) || (pluginFinalize.tokenLookup2 != NULL)) {
            if (pluginFinalize.tokenLookup1 != NULL) {
                PRINTF("Lookup1: %.*H\n", ADDRESS_LENGTH, pluginFinalize.tokenLookup1);
                token1 = getKnownToken(pluginFinalize.tokenLookup1);
                if (token1 != NULL) {
                    PRINTF("Token1 ticker: %s\n", token1->ticker);
                }
            }
            if (pluginFinalize.tokenLookup2 != NULL) {
                PRINTF("Lookup2: %.*H\n", ADDRESS_LENGTH, pluginFinalize.tokenLookup2);
                token2 = getKnownToken(pluginFinalize.tokenLookup2);
                if (token2 != NULL) {
                    PRINTF("Token2 ticker: %s\n", token2->ticker);
                }
            }
            eth_plugin_prepare_provide_token(&pluginProvideToken, token1, token2);
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
                    tmpContent.txContent.dataPresent = false;
                    // Add the number of screens + the number of additional screens to get the total
                    // number of screens needed.
                    dataContext.tokenContext.pluginUiMaxItems =
                        pluginFinalize.numScreens + pluginProvideToken.additionalScreens;
                    break;
                case ETH_UI_TYPE_AMOUNT_ADDRESS:
                    genericUI = true;
                    tmpContent.txContent.dataPresent = false;
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
                    if (token1 != NULL) {
                        decimals = token1->decimals;
                        ticker = token1->ticker;
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

    if (tmpContent.txContent.dataPresent && !N_storage.dataAllowed) {
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
                                          (uint8_t *) displayBuffer + 2,
                                          &global_sha3,
                                          chainConfig);
            compareOrCopy(strings.common.fullAddress, displayBuffer, called_from_swap);
        } else {
            strcpy(strings.common.fullAddress, "Contract");
        }
    }
    // Prepare amount to display
    if (genericUI) {
        amountToString(tmpContent.txContent.value.value,
                       tmpContent.txContent.value.length,
                       decimals,
                       (char *) ticker,
                       displayBuffer,
                       sizeof(displayBuffer));
        compareOrCopy(strings.common.fullAmount, displayBuffer, called_from_swap);
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
        prepareFeeDisplay();
    }

    // Prepare chainID field
    if (genericUI) {
        prepareChainIdDisplay();
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
            ux_approve_tx(false);
        } else {
            plugin_ui_start();
        }
    }
}
