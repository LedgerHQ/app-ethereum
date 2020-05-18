#include "shared_context.h"
#include "utils.h"
#include "ui_callbacks.h"
#ifdef TARGET_BLUE
#include "ui_blue.h"
#endif
#ifdef HAVE_UX_FLOW
#include "ui_flow.h"
#endif

#define TOKEN_TRANSFER_DATA_SIZE 4 + 32 + 32
static const uint8_t const TOKEN_TRANSFER_ID[] = { 0xa9, 0x05, 0x9c, 0xbb };

#define ALLOWANCE_DATA_SIZE 4 + 32 + 32
static const uint8_t const ALLOWANCE_ID[] = { 0x09, 0x5e, 0xa7, 0xb3 };

uint32_t splitBinaryParameterPart(char *result, uint8_t *parameter) {
    uint32_t i;
    for (i=0; i<8; i++) {
        if (parameter[i] != 0x00) {
            break;
        }
    }
    if (i == 8) {
        result[0] = '0';
        result[1] = '0';
        result[2] = '\0';
        return 2;
    }
    else {
        array_hexstr(result, parameter + i, 8 - i);
        return ((8 - i) * 2);
    }
}

customStatus_e customProcessor(txContext_t *context) {
    if ((context->currentField == TX_RLP_DATA) &&
        (context->currentFieldLength != 0)) {
        dataPresent = true;
        // If handling a new contract rather than a function call, abort immediately
        if (tmpContent.txContent.destinationLength == 0) {
            return CUSTOM_NOT_HANDLED;
        }
        if (context->currentFieldPos == 0) {
            // If handling the beginning of the data field, assume that the function selector is present
            if (context->commandLength < 4) {
                PRINTF("Missing function selector\n");
                return CUSTOM_FAULT;
            }
            // Initial check to see if the call can be processed
            if ((context->currentFieldLength == TOKEN_TRANSFER_DATA_SIZE) &&
                (os_memcmp(context->workBuffer, TOKEN_TRANSFER_ID, 4) == 0) &&
                (getKnownToken(tmpContent.txContent.destination) != NULL)) {
              contractProvisioned = CONTRACT_ERC20;
            }
            else
            if ((context->currentFieldLength == ALLOWANCE_DATA_SIZE) &&
                (os_memcmp(context->workBuffer, ALLOWANCE_ID, 4) == 0)) {
              contractProvisioned = CONTRACT_ALLOWANCE;
            }
        }
        // Sanity check
        if ((contractProvisioned != CONTRACT_NONE) && (context->currentFieldLength > sizeof(dataContext.tokenContext.data))) {
          PRINTF("Data field overflow - dropping customization\n");
          contractProvisioned = CONTRACT_NONE;
        }
        PRINTF("contractProvisioned %d\n", contractProvisioned);
        if (contractProvisioned != CONTRACT_NONE) {
            if (context->currentFieldPos < context->currentFieldLength) {
                uint32_t copySize = (context->commandLength <
                                        ((context->currentFieldLength -
                                                   context->currentFieldPos))
                                        ? context->commandLength
                                            : context->currentFieldLength -
                                                   context->currentFieldPos);
                copyTxData(context,
                    dataContext.tokenContext.data + context->currentFieldPos,
                    copySize);
            }
            if (context->currentFieldPos == context->currentFieldLength) {
                context->currentField++;
                context->processingField = false;
            }
            return CUSTOM_HANDLED;
        }
        else {
            uint32_t blockSize;
            uint32_t copySize;
            uint32_t fieldPos = context->currentFieldPos;
            if (fieldPos == 0) {
                if (!N_storage.dataAllowed) {
                  PRINTF("Data field forbidden\n");
                  return CUSTOM_FAULT;
                }
                if (!N_storage.contractDetails) {
                  return CUSTOM_NOT_HANDLED;
                }
                dataContext.rawDataContext.fieldIndex = 0;
                dataContext.rawDataContext.fieldOffset = 0;
                blockSize = 4;
            }
            else {
                if (!N_storage.contractDetails) {
                  return CUSTOM_NOT_HANDLED;
                }
                blockSize = 32 - (dataContext.rawDataContext.fieldOffset % 32);
            }

            // Sanity check
            if ((context->currentFieldLength - fieldPos) < blockSize) {
                PRINTF("Unconsistent data\n");
                return CUSTOM_FAULT;
            }

            copySize = (context->commandLength < blockSize ? context->commandLength : blockSize);
            copyTxData(context,
                        dataContext.rawDataContext.data + dataContext.rawDataContext.fieldOffset,
                        copySize);

            if (context->currentFieldPos == context->currentFieldLength) {
                context->currentField++;
                context->processingField = false;
            }

            dataContext.rawDataContext.fieldOffset += copySize;

            if (copySize == blockSize) {
                // Can display
                if (fieldPos != 0) {
                    dataContext.rawDataContext.fieldIndex++;
                }
                dataContext.rawDataContext.fieldOffset = 0;
                if (fieldPos == 0) {
                    array_hexstr(strings.tmp.tmp, dataContext.rawDataContext.data, 4);
#if defined(TARGET_BLUE)
                    UX_DISPLAY(ui_data_selector_blue, ui_data_selector_blue_prepro);
#else
                    ux_flow_init(0, ux_confirm_selector_flow, NULL);
#endif // #if TARGET_ID
                }
                else {
                    uint32_t offset = 0;
                    uint32_t i;
                    snprintf(strings.tmp.tmp2, sizeof(strings.tmp.tmp2), "Field %d", dataContext.rawDataContext.fieldIndex);
                    for (i=0; i<4; i++) {
                        offset += splitBinaryParameterPart(strings.tmp.tmp + offset, dataContext.rawDataContext.data + 8 * i);
                        if (i != 3) {
                            strings.tmp.tmp[offset++] = ':';
                        }
                    }
#if defined(TARGET_BLUE)
                    UX_DISPLAY(ui_data_parameter_blue, ui_data_parameter_blue_prepro);
#else
                    ux_flow_init(0, ux_confirm_parameter_flow, NULL);
#endif // #if TARGET_ID
                }
            }
            else {
                return CUSTOM_HANDLED;
            }

            return CUSTOM_SUSPENDED;
        }
    }
    return CUSTOM_NOT_HANDLED;
}

void finalizeParsing(bool direct) {
  uint256_t gasPrice, startGas, uint256;
  uint32_t i;
  uint8_t address[41];
  uint8_t decimals = WEI_TO_ETHER;
  uint8_t *ticker = (uint8_t *)PIC(chainConfig->coinName);
  uint8_t *feeTicker = (uint8_t *)PIC(chainConfig->coinName);
  uint8_t tickerOffset = 0;

  // Verify the chain
  if (chainConfig->chainId != 0) {
    uint32_t v = getV(&tmpContent.txContent);
    if (chainConfig->chainId != v) {
        reset_app_context();
        PRINTF("Invalid chainId %d expected %d\n", v, chainConfig->chainId);
        if (direct) {
            THROW(0x6A80);
        }
        else {
            io_seproxyhal_send_status(0x6A80);
            ui_idle();
            return;
        }
    }
  }
  // Store the hash
  cx_hash((cx_hash_t *)&sha3, CX_LAST, tmpCtx.transactionContext.hash, 0, tmpCtx.transactionContext.hash, 32);
    // If there is a token to process, check if it is well known
    if ((contractProvisioned == CONTRACT_ERC20) || (contractProvisioned == CONTRACT_ALLOWANCE)) {
        tokenDefinition_t *currentToken = getKnownToken(tmpContent.txContent.destination);
        if (currentToken != NULL) {
            dataPresent = false;
            decimals = currentToken->decimals;
            ticker = currentToken->ticker;
            tmpContent.txContent.destinationLength = 20;
            os_memmove(tmpContent.txContent.destination, dataContext.tokenContext.data + 4 + 12, 20);
            os_memmove(tmpContent.txContent.value.value, dataContext.tokenContext.data + 4 + 32, 32);
            tmpContent.txContent.value.length = 32;
        }
    }
    else {
      if (dataPresent && contractProvisioned == CONTRACT_NONE && !N_storage.dataAllowed) {
          reset_app_context();
          PRINTF("Data field forbidden\n");
          if (direct) {
            THROW(0x6A80);
          }
          else {
            io_seproxyhal_send_status(0x6A80);
            ui_idle();
            return;
          }
      }
    }
  // Add address
  if (tmpContent.txContent.destinationLength != 0) {
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &sha3);
    /*
    addressSummary[0] = '0';
    addressSummary[1] = 'x';
    os_memmove((unsigned char *)(addressSummary + 2), address, 4);
    os_memmove((unsigned char *)(addressSummary + 6), "...", 3);
    os_memmove((unsigned char *)(addressSummary + 9), address + 40 - 4, 4);
    addressSummary[13] = '\0';
    */

    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
  }
  else
  {
#ifdef TARGET_BLUE
    os_memmove((void*)addressSummary, CONTRACT_ADDRESS, sizeof(CONTRACT_ADDRESS));
#endif
    strcpy(strings.common.fullAddress, "Contract");
  }
  if ((contractProvisioned == CONTRACT_NONE) || (contractProvisioned == CONTRACT_ERC20) ||
    (contractProvisioned == CONTRACT_ALLOWANCE)) {
    // Add amount in ethers or tokens
    if ((contractProvisioned == CONTRACT_ALLOWANCE) && ismaxint(tmpContent.txContent.value.value, 32)) {
      strcpy((char*)G_io_apdu_buffer, "Unlimited");
    }
    else {
      convertUint256BE(tmpContent.txContent.value.value, tmpContent.txContent.value.length, &uint256);
      tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100), 100);
      i = 0;
      while (G_io_apdu_buffer[100 + i]) {
        i++;
      }
      adjustDecimals((char *)(G_io_apdu_buffer + 100), i, (char *)G_io_apdu_buffer, 100, decimals);
    }
    i = 0;
    tickerOffset = 0;
    while (ticker[tickerOffset]) {
      strings.common.fullAmount[tickerOffset] = ticker[tickerOffset];
      tickerOffset++;
    }
    while (G_io_apdu_buffer[i]) {
      strings.common.fullAmount[tickerOffset + i] = G_io_apdu_buffer[i];
      i++;
    }
    strings.common.fullAmount[tickerOffset + i] = '\0';
  }
  // Compute maximum fee
  convertUint256BE(tmpContent.txContent.gasprice.value, tmpContent.txContent.gasprice.length, &gasPrice);
  convertUint256BE(tmpContent.txContent.startgas.value, tmpContent.txContent.startgas.length, &startGas);
  mul256(&gasPrice, &startGas, &uint256);
  tostring256(&uint256, 10, (char *)(G_io_apdu_buffer + 100), 100);
  i = 0;
  while (G_io_apdu_buffer[100 + i]) {
    i++;
  }
  adjustDecimals((char *)(G_io_apdu_buffer + 100), i, (char *)G_io_apdu_buffer, 100, WEI_TO_ETHER);
  i = 0;
  tickerOffset=0;
  while (feeTicker[tickerOffset]) {
      strings.common.maxFee[tickerOffset] = feeTicker[tickerOffset];
      tickerOffset++;
  }
  tickerOffset++;
  while (G_io_apdu_buffer[i]) {
    strings.common.maxFee[tickerOffset + i] = G_io_apdu_buffer[i];
    i++;
  }
  strings.common.maxFee[tickerOffset + i] = '\0';

#ifdef NO_CONSENT
  io_seproxyhal_touch_tx_ok(NULL);
#else // NO_CONSENT
#if defined(TARGET_BLUE)
  ui_approval_transaction_blue_init();
#else

  if (contractProvisioned == CONTRACT_ALLOWANCE) {
    ux_flow_init(0, ux_approval_allowance_flow, NULL);
    return;
  }

  ux_flow_init(0,
    ((dataPresent && !N_storage.contractDetails) ? ux_approval_tx_data_warning_flow : ux_approval_tx_flow),
    NULL);
#endif // #if TARGET_ID
#endif // NO_CONSENT
}

