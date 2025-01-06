#include <ctype.h>
#include "shared_context.h"
#include "common_utils.h"
#include "feature_signTx.h"
#include "uint256.h"
#include "eth_plugin_handler.h"
#include "network.h"
#include "common_ui.h"
#include "ui_callbacks.h"
#include "apdu_constants.h"
#include "crypto_helpers.h"
#include "format.h"
#include "manage_asset_info.h"
#include "handle_swap_sign_transaction.h"
#include "os_math.h"
#include "calldata.h"

static bool g_use_standard_ui;

static uint32_t splitBinaryParameterPart(char *result, size_t result_size, uint8_t *parameter) {
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
        format_hex(parameter + i, 8 - i, result, result_size);
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
            // Still go through plugin activation in Swap context
#ifdef HAVE_GENERIC_TX_PARSER
            if (!context->store_calldata) {
#else
            {
#endif
                if (!N_storage.contractDetails || G_called_from_swap) {
                    eth_plugin_prepare_init(&pluginInit,
                                            context->workBuffer,
                                            context->currentFieldLength);
                    dataContext.tokenContext.pluginStatus =
                        eth_plugin_perform_init(tmpContent.txContent.destination, &pluginInit);
                }
            }
            PRINTF("pluginstatus %d\n", dataContext.tokenContext.pluginStatus);
            eth_plugin_result_t status = dataContext.tokenContext.pluginStatus;
            if (status == ETH_PLUGIN_RESULT_ERROR) {
                PRINTF("Plugin error\n");
                return CUSTOM_FAULT;
            } else if (status >= ETH_PLUGIN_RESULT_SUCCESSFUL) {
                dataContext.tokenContext.fieldIndex = 0;
                dataContext.tokenContext.fieldOffset = 0;
                if (copyTxData(context, NULL, 4) == false) {
                    return CUSTOM_FAULT;
                }
                if (context->currentFieldLength == 4) {
                    return CUSTOM_NOT_HANDLED;
                }
            }
        }
        uint32_t blockSize;
        uint32_t copySize;
        uint32_t fieldPos = context->currentFieldPos;
        if (fieldPos == 0) {  // not reached if a plugin is available
#ifdef HAVE_GENERIC_TX_PARSER
            if (!context->store_calldata) {
#else
            {
#endif
                if (!N_storage.dataAllowed) {
                    PRINTF("Data field forbidden\n");
                    ui_error_blind_signing();
                    return CUSTOM_FAULT;
                }
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

        // If the last parameter is of type `bytes` then we might have an
        // edge case where the data is not a multiple of 32. Set `blockSize` accordingly
        if ((context->currentFieldLength - fieldPos) < blockSize) {
            blockSize = context->currentFieldLength - fieldPos;
        }

        copySize = (context->commandLength < blockSize ? context->commandLength : blockSize);

        PRINTF("currentFieldPos %d copySize %d\n", context->currentFieldPos, copySize);

        if (copyTxData(context,
                       dataContext.tokenContext.data + dataContext.tokenContext.fieldOffset,
                       copySize) == false) {
            return CUSTOM_FAULT;
        }

        if (context->currentFieldPos == context->currentFieldLength) {
            PRINTF("\n\nIncrementing one\n");
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
                memset(dataContext.tokenContext.data, 0, sizeof(dataContext.tokenContext.data));
                return CUSTOM_HANDLED;
            }

            if (fieldPos != 0) {
                dataContext.tokenContext.fieldIndex++;
            }
            dataContext.tokenContext.fieldOffset = 0;
            if (fieldPos == 0) {
                format_hex(dataContext.tokenContext.data,
                           4,
                           strings.tmp.tmp,
                           sizeof(strings.tmp.tmp));
                ui_confirm_selector();
            } else {
                uint32_t offset = 0;
                uint32_t i;
                snprintf(strings.tmp.tmp2,
                         sizeof(strings.tmp.tmp2),
                         "Field %d",
                         dataContext.tokenContext.fieldIndex);
                for (i = 0; i < 4; i++) {
                    offset += splitBinaryParameterPart(strings.tmp.tmp + offset,
                                                       sizeof(strings.tmp.tmp) - offset,
                                                       dataContext.tokenContext.data + 8 * i);
                    if (i != 3) {
                        strings.tmp.tmp[offset++] = ':';
                    }
                }
                ui_confirm_parameter();
            }
        } else {
            return CUSTOM_HANDLED;
        }

        return CUSTOM_SUSPENDED;
    }
    return CUSTOM_NOT_HANDLED;
}

static void report_finalize_error(void) {
    io_seproxyhal_send_status(APDU_RESPONSE_INVALID_DATA, 0, true, true);
}

static uint16_t address_to_string(uint8_t *in,
                                  size_t in_len,
                                  char *out,
                                  size_t out_len,
                                  uint64_t chainId) {
    if (in_len != 0) {
        if (!getEthDisplayableAddress(in, out, out_len, chainId)) {
            return APDU_RESPONSE_ERROR_NO_INFO;
        }
    } else {
        strlcpy(out, "Contract", out_len);
    }
    return APDU_RESPONSE_OK;
}

static void raw_fee_to_string(uint256_t *rawFee, char *displayBuffer, uint32_t displayBufferSize) {
    uint64_t chain_id = get_tx_chain_id();
    const char *feeTicker = get_displayable_ticker(&chain_id, chainConfig);
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
        if ((uint32_t) tickerOffset >= displayBufferSize) {
            break;
        }

        displayBuffer[tickerOffset] = feeTicker[tickerOffset];
        tickerOffset++;
    }
    if ((uint32_t) tickerOffset < displayBufferSize) displayBuffer[tickerOffset++] = ' ';
    while (G_io_apdu_buffer[i]) {
        if ((uint32_t) (tickerOffset) + i >= displayBufferSize) {
            break;
        }
        displayBuffer[tickerOffset + i] = G_io_apdu_buffer[i];
        i++;
    }

    if ((uint32_t) (tickerOffset) + i < displayBufferSize) {
        displayBuffer[tickerOffset + i] = '\0';
    }
}

// Compute the fees, transform it to a string, prepend a ticker to it and copy everything to
// `displayBuffer` output
bool max_transaction_fee_to_string(const txInt256_t *BEGasPrice,
                                   const txInt256_t *BEGasLimit,
                                   char *displayBuffer,
                                   uint32_t displayBufferSize) {
    // Use temporary variables to convert values to uint256_t
    uint256_t gasPrice = {0};
    uint256_t gasLimit = {0};
    // Use temporary variable to store the result of the operation in uint256_t
    uint256_t rawFee = {0};

    PRINTF("Gas price %.*H\n", BEGasPrice->length, BEGasPrice->value);
    PRINTF("Gas limit %.*H\n", BEGasLimit->length, BEGasLimit->value);
    convertUint256BE(BEGasPrice->value, BEGasPrice->length, &gasPrice);
    convertUint256BE(BEGasLimit->value, BEGasLimit->length, &gasLimit);
    if (mul256(&gasPrice, &gasLimit, &rawFee) == false) {
        return false;
    }
    raw_fee_to_string(&rawFee, displayBuffer, displayBufferSize);
    return true;
}

static void nonce_to_string(const txInt256_t *nonce, char *out, size_t out_size) {
    uint256_t nonce_uint256;
    convertUint256BE(nonce->value, nonce->length, &nonce_uint256);
    tostring256(&nonce_uint256, 10, out, out_size);
}

uint16_t get_network_as_string(char *out, size_t out_size) {
    uint64_t chain_id = get_tx_chain_id();
    const char *name = get_network_name_from_chain_id(&chain_id);

    if (name == NULL) {
        // No network name found so simply copy the chain ID as the network name.
        if (!u64_to_string(chain_id, out, out_size)) {
            return APDU_RESPONSE_CHAINID_OUT_BUF_SMALL;
        }
    } else {
        // Network name found, simply copy it.
        strlcpy(out, name, out_size);
    }
    return APDU_RESPONSE_OK;
}

uint16_t get_public_key(uint8_t *out, uint8_t outLength) {
    uint8_t raw_pubkey[65];
    cx_err_t error = CX_INTERNAL_ERROR;

    if (outLength < ADDRESS_LENGTH) {
        return APDU_RESPONSE_WRONG_DATA_LENGTH;
    }
    CX_CHECK(bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                         tmpCtx.transactionContext.bip32.path,
                                         tmpCtx.transactionContext.bip32.length,
                                         raw_pubkey,
                                         NULL,
                                         CX_SHA512));

    getEthAddressFromRawKey(raw_pubkey, out);
    error = APDU_RESPONSE_OK;
end:
    return error;
}

/* Local implementation of strncasecmp, workaround of the segfaulting base implem
 * Remove once strncasecmp is fixed
 */
static int strcasecmp_workaround(const char *str1, const char *str2) {
    unsigned char c1, c2;
    do {
        c1 = *str1++;
        c2 = *str2++;
        if (toupper(c1) != toupper(c2)) {
            return toupper(c1) - toupper(c2);
        }
    } while (c1 != '\0');
    return 0;
}

__attribute__((noreturn)) void send_swap_error(uint8_t error_code,
                                               app_code_t app_code,
                                               const char *str1,
                                               const char *str2) {
    uint32_t tx = 0;
    size_t len = 0;
    PRINTF("APDU_RESPONSE_MODE_CHECK_FAILED: 0x%x\n", error_code);
    // Set RAPDU error codes
    G_io_apdu_buffer[tx++] = error_code;
    G_io_apdu_buffer[tx++] = app_code;
    // Set RAPDU error message
    if (str1 != NULL) {
        PRINTF("Expected %s\n", str1);
        // If the string is too long, truncate it
        len = MIN(strlen((const char *) str1), sizeof(G_io_apdu_buffer) - tx - 2);
        memmove(G_io_apdu_buffer + tx, str1, len);
        tx += len;
        if (len < strlen((const char *) str1)) {
            PRINTF("Truncated %s to %d bytes\n", str1, len);
            G_io_apdu_buffer[tx - 1] = '*';
        }
    }
    if (str2 != NULL) {
        PRINTF("Received %s\n", str2);
        // Do we have enough space to add a separator?
        if ((tx + 1 + 2) < sizeof(G_io_apdu_buffer)) {
            G_io_apdu_buffer[tx++] = '#';
        }
        // Do we have enough space to add at least one character?
        if ((tx + 1 + 2) < sizeof(G_io_apdu_buffer)) {
            // If the string is too long, truncate it
            len = MIN(strlen((const char *) str2), sizeof(G_io_apdu_buffer) - tx - 2);
            memmove(G_io_apdu_buffer + tx, str2, len);
            tx += len;
            if (len < strlen((const char *) str2)) {
                PRINTF("Truncated %s to %d bytes\n", str2, len);
                G_io_apdu_buffer[tx - 1] = '*';
            }
        }
    }
    // Set RAPDU status word, with previous check we are sure there is at least 2 bytes left
    U2BE_ENCODE(G_io_apdu_buffer, tx, APDU_RESPONSE_MODE_CHECK_FAILED);
    tx += 2;
    // Send RAPDU
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // In case of success, the apdu is sent immediately and eth exits
    // Reaching this code means we encountered an error
    finalize_exchange_sign_transaction(false);
}

__attribute__((noinline)) static uint16_t finalize_parsing_helper(const txContext_t *context) {
    char displayBuffer[50];
    uint8_t decimals = WEI_TO_ETHER;
    uint64_t chain_id = get_tx_chain_id();
    const char *ticker = get_displayable_ticker(&chain_id, chainConfig);
    ethPluginFinalize_t pluginFinalize;
    cx_err_t error = CX_INTERNAL_ERROR;

    // Verify the chain
    if (chainConfig->chainId != ETHEREUM_MAINNET_CHAINID) {
        uint64_t id = get_tx_chain_id();

        if (chainConfig->chainId != id) {
            PRINTF("Invalid chainID %u expected %u\n", id, chainConfig->chainId);
            report_finalize_error();
            return APDU_NO_RESPONSE;
        }
    }
    // Store the hash
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              CX_LAST,
                              tmpCtx.transactionContext.hash,
                              0,
                              tmpCtx.transactionContext.hash,
                              32));

    uint8_t msg_sender[ADDRESS_LENGTH] = {0};
    error = get_public_key(msg_sender, sizeof(msg_sender));
    if (error != APDU_RESPONSE_OK) {
        goto end;
    }

    error = address_to_string(msg_sender,
                              ADDRESS_LENGTH,
                              strings.common.fromAddress,
                              sizeof(strings.common.fromAddress),
                              chainConfig->chainId);
    if (error != APDU_RESPONSE_OK) {
        goto end;
    }
    PRINTF("FROM address displayed: %s\n", strings.common.fromAddress);
    // Finalize the plugin handling
    if (dataContext.tokenContext.pluginStatus >= ETH_PLUGIN_RESULT_SUCCESSFUL) {
        eth_plugin_prepare_finalize(&pluginFinalize);

        pluginFinalize.address = msg_sender;

        if (!eth_plugin_call(ETH_PLUGIN_FINALIZE, (void *) &pluginFinalize)) {
            PRINTF("Plugin finalize call failed\n");
            report_finalize_error();
            return APDU_NO_RESPONSE;
        }
        // Lookup tokens if requested
        ethPluginProvideInfo_t pluginProvideInfo;
        eth_plugin_prepare_provide_info(&pluginProvideInfo);
        if ((pluginFinalize.tokenLookup1 != NULL) || (pluginFinalize.tokenLookup2 != NULL)) {
            if (pluginFinalize.tokenLookup1 != NULL) {
                PRINTF("Lookup1: %.*H\n", ADDRESS_LENGTH, pluginFinalize.tokenLookup1);
                pluginProvideInfo.item1 = get_asset_info_by_addr(pluginFinalize.tokenLookup1);
                if (pluginProvideInfo.item1 != NULL) {
                    PRINTF("Token1 ticker: %s\n", pluginProvideInfo.item1->token.ticker);
                }
            }
            if (pluginFinalize.tokenLookup2 != NULL) {
                PRINTF("Lookup2: %.*H\n", ADDRESS_LENGTH, pluginFinalize.tokenLookup2);
                pluginProvideInfo.item2 = get_asset_info_by_addr(pluginFinalize.tokenLookup2);
                if (pluginProvideInfo.item2 != NULL) {
                    PRINTF("Token2 ticker: %s\n", pluginProvideInfo.item2->token.ticker);
                }
            }
            if (eth_plugin_call(ETH_PLUGIN_PROVIDE_INFO, (void *) &pluginProvideInfo) <=
                ETH_PLUGIN_RESULT_UNSUCCESSFUL) {
                PRINTF("Plugin provide token call failed\n");
                report_finalize_error();
                return APDU_NO_RESPONSE;
            }
            pluginFinalize.result = pluginProvideInfo.result;
        }
        if (pluginFinalize.result != ETH_PLUGIN_RESULT_FALLBACK) {
            PRINTF("pluginFinalize.result %d successful\n", pluginFinalize.result);
            // Handle the right interface
            switch (pluginFinalize.uiType) {
                case ETH_UI_TYPE_GENERIC:
                    // Use the dedicated ETH plugin UI
                    g_use_standard_ui = false;
                    tmpContent.txContent.dataPresent = false;
                    // Add the number of screens + the number of additional screens to get the total
                    // number of screens needed.
                    dataContext.tokenContext.pluginUiMaxItems =
                        pluginFinalize.numScreens + pluginProvideInfo.additionalScreens;
                    break;
                case ETH_UI_TYPE_AMOUNT_ADDRESS:
                    // Use the standard ETH UI as this plugin uses the amount/address UI
                    g_use_standard_ui = true;
                    tmpContent.txContent.dataPresent = false;
                    if ((pluginFinalize.amount == NULL) || (pluginFinalize.address == NULL)) {
                        PRINTF("Incorrect amount/address set by plugin\n");
                        report_finalize_error();
                        return APDU_NO_RESPONSE;
                    }
                    memmove(tmpContent.txContent.value.value, pluginFinalize.amount, 32);
                    tmpContent.txContent.value.length = 32;
                    memmove(tmpContent.txContent.destination, pluginFinalize.address, 20);
                    tmpContent.txContent.destinationLength = 20;
                    if (pluginProvideInfo.item1 != NULL) {
                        decimals = pluginProvideInfo.item1->token.decimals;
                        ticker = pluginProvideInfo.item1->token.ticker;
                    }
                    break;
                default:
                    PRINTF("ui type %d not supported\n", pluginFinalize.uiType);
                    report_finalize_error();
                    return APDU_NO_RESPONSE;
            }
        } else if (G_called_from_swap && G_swap_mode == SWAP_MODE_CROSSCHAIN_SUCCESS) {
            PRINTF("Plugin swap_with_calldata fell back for UI with success\n");
            // We are not bling signing, the data has been validated by the plugin
            tmpContent.txContent.dataPresent = false;
        }
    }

    if (G_called_from_swap) {
        if (G_swap_response_ready) {
            // Unreachable given current return to exchange mechanism. Safeguard against regression
            PRINTF("FATAL: safety against double sign triggered\n");
            app_exit();
        }
        G_swap_response_ready = true;
    }

    if (G_called_from_swap) {
        // User has just validated a swap but ETH received apdus about a non standard plugin /
        // contract
        if (!g_use_standard_ui) {
            send_swap_error(ERROR_WRONG_METHOD, APP_CODE_NO_STANDARD_UI, NULL, NULL);
            // unreachable
            os_sched_exit(0);
        }
        // Two success cases: we are in standard mode and no calldata was received
        // We are in crosschain mode and the correct calldata has been received
        if (G_swap_mode != SWAP_MODE_STANDARD && G_swap_mode != SWAP_MODE_CROSSCHAIN_SUCCESS) {
            send_swap_error(ERROR_CROSSCHAIN_WRONG_MODE, APP_CODE_DEFAULT, NULL, NULL);
            // unreachable
            os_sched_exit(0);
        }
    }

#ifdef HAVE_GENERIC_TX_PARSER
    if (!context->store_calldata) {
#else
    (void) context;
    {
#endif
        if (tmpContent.txContent.dataPresent && !N_storage.dataAllowed) {
            PRINTF("Data is present but not allowed\n");
            report_finalize_error();
            ui_error_blind_signing();
            return false;
        }
    }

    // Prepare destination address and amount to display
    if (g_use_standard_ui) {
        // Format the address in a temporary buffer, if in swap case compare it with validated
        // address, else commit it
        error = address_to_string(tmpContent.txContent.destination,
                                  tmpContent.txContent.destinationLength,
                                  displayBuffer,
                                  sizeof(displayBuffer),
                                  chainConfig->chainId);
        if (error != APDU_RESPONSE_OK) {
            goto end;
        }
        if (G_called_from_swap) {
            // Ensure the values are the same that the ones that have been previously validated
            if (strcasecmp_workaround(strings.common.toAddress, displayBuffer) != 0) {
                send_swap_error(ERROR_WRONG_DESTINATION,
                                APP_CODE_DEFAULT,
                                strings.common.toAddress,
                                displayBuffer);
                // unreachable
                os_sched_exit(0);
            }
        } else {
            strlcpy(strings.common.toAddress, displayBuffer, sizeof(strings.common.toAddress));
        }
        PRINTF("TO address displayed: %s\n", strings.common.toAddress);

        // Format the amount in a temporary buffer, if in swap case compare it with validated
        // amount, else commit it
        if (!amountToString(tmpContent.txContent.value.value,
                            tmpContent.txContent.value.length,
                            decimals,
                            ticker,
                            displayBuffer,
                            sizeof(displayBuffer))) {
            PRINTF("OVERFLOW, amount to string failed\n");
            return EXCEPTION_OVERFLOW;
        }

        if (G_called_from_swap) {
            // Ensure the values are the same that the ones that have been previously validated
            if (strcmp(strings.common.fullAmount, displayBuffer) != 0) {
                send_swap_error(ERROR_WRONG_AMOUNT,
                                APP_CODE_DEFAULT,
                                strings.common.fullAmount,
                                displayBuffer);
                // unreachable
                os_sched_exit(0);
            }
        } else {
            strlcpy(strings.common.fullAmount, displayBuffer, sizeof(strings.common.fullAmount));
        }
        PRINTF("Amount displayed: %s\n", strings.common.fullAmount);
    }

    // Compute the max fee in a temporary buffer, if in swap case compare it with validated max fee,
    // else commit it
    if (max_transaction_fee_to_string(&tmpContent.txContent.gasprice,
                                      &tmpContent.txContent.startgas,
                                      displayBuffer,
                                      sizeof(displayBuffer)) == false) {
        return APDU_RESPONSE_INVALID_DATA;
    }
    if (G_called_from_swap) {
        // Ensure the values are the same that the ones that have been previously validated
        if (strcmp(strings.common.maxFee, displayBuffer) != 0) {
            send_swap_error(ERROR_WRONG_FEES,
                            APP_CODE_DEFAULT,
                            strings.common.maxFee,
                            displayBuffer);
            // unreachable
            os_sched_exit(0);
        }
    } else {
        strlcpy(strings.common.maxFee, displayBuffer, sizeof(strings.common.maxFee));
    }

    PRINTF("Fees displayed: %s\n", strings.common.maxFee);

    // Prepare nonce to display
    nonce_to_string(&tmpContent.txContent.nonce,
                    strings.common.nonce,
                    sizeof(strings.common.nonce));
    PRINTF("Nonce: %s\n", strings.common.nonce);

    // Prepare network field
    error = get_network_as_string(strings.common.network_name, sizeof(strings.common.network_name));
    if (error == APDU_RESPONSE_OK) {
        PRINTF("Network: %s\n", strings.common.network_name);
    }
end:
    return error;
}

void start_signature_flow(void) {
    if (g_use_standard_ui) {
        ux_approve_tx(false);
    } else {
        dataContext.tokenContext.pluginUiState = PLUGIN_UI_OUTSIDE;
        dataContext.tokenContext.pluginUiCurrentItem = 0;
        ux_approve_tx(true);
    }
}

uint16_t finalize_parsing(const txContext_t *context) {
    uint16_t sw = APDU_RESPONSE_UNKNOWN;
    g_use_standard_ui = true;

    sw = finalize_parsing_helper(context);
    if (sw != APDU_RESPONSE_OK) {
        return sw;
    }
#ifdef HAVE_GENERIC_TX_PARSER
    if (context->store_calldata) {
        if (calldata_get_selector() == NULL) {
            PRINTF("Asked to store calldata but none was provided!\n");
            return APDU_RESPONSE_INVALID_DATA;
        }
    } else {
#else
    (void) context;
    {
#endif
        // If called from swap, the user has already validated a standard transaction
        // And we have already checked the fields of this transaction above
        if (G_called_from_swap && g_use_standard_ui) {
            ui_idle();
            io_seproxyhal_touch_tx_ok();
        } else {
#ifdef HAVE_BAGL
            // If blind-signing detected, start the warning flow beforehand
            if (tmpContent.txContent.dataPresent) {
                ui_warning_blind_signing();
            } else
#endif
            {
                start_signature_flow();
            }
        }
    }
    return APDU_RESPONSE_OK;
}
