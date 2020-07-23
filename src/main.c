/*******************************************************************************
*   Ledger Ethereum App
*   (c) 2016-2019 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "shared_context.h"
#include "apdu_constants.h"
#include "ui_callbacks.h"
#ifdef TARGET_BLUE
#include "ui_blue.h"
#endif
#ifdef HAVE_UX_FLOW
#include "ui_flow.h"
#endif

#include "os_io_seproxyhal.h"

#include "glyphs.h"
#include "utils.h"

#ifdef HAVE_STARKWARE
#include "stark_crypto.h"
#endif

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

void ui_idle(void);

uint32_t set_result_get_publicKey(void);
void finalizeParsing(bool);

tmpCtx_t tmpCtx;
txContext_t txContext;
tmpContent_t tmpContent;
dataContext_t dataContext;
strings_t strings;

cx_sha3_t sha3;

uint8_t dataAllowed;
uint8_t contractDetails;
uint8_t appState;
#ifdef TARGET_BLUE
char addressSummary[32];
#endif
bool dataPresent;
contract_call_t contractProvisioned;
#ifdef HAVE_STARKWARE
bool quantumSet;
#endif

#ifdef HAVE_UX_FLOW
#include "ux.h"
ux_state_t G_ux;
bolos_ux_params_t G_ux_params;
#else // HAVE_UX_FLOW
ux_state_t ux;

// display stepped screens
unsigned int ux_step;
unsigned int ux_step_count;
#endif // HAVE_UX_FLOW

const internalStorage_t N_storage_real;

#ifdef TARGET_BLUE
static const char const CONTRACT_ADDRESS[] = "New contract";
#endif


chain_config_t *chainConfig;

void reset_app_context() {
  PRINTF("!!RESET_APP_CONTEXT\n");
  appState = APP_STATE_IDLE;
  os_memset(tmpCtx.transactionContext.tokenSet, 0, MAX_TOKEN);
  contractProvisioned = CONTRACT_NONE;
#ifdef HAVE_STARKWARE  
  quantumSet = false;
#endif  
  os_memset((uint8_t*)&txContext, 0, sizeof(txContext));
  os_memset((uint8_t*)&tmpContent, 0, sizeof(tmpContent));
}

void ui_idle(void) {
#if defined(TARGET_BLUE)
    UX_DISPLAY(ui_idle_blue, ui_idle_blue_prepro);
#elif defined(HAVE_UX_FLOW)
    // reserve a display stack slot if none yet
    if(G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
#endif // #if TARGET_ID
}

#if defined(TARGET_BLUE)
unsigned int io_seproxyhal_touch_settings(const bagl_element_t *e) {
  UX_DISPLAY(ui_settings_blue, ui_settings_blue_prepro);
  return 0; // do not redraw button, screen has switched
}
#endif // #if defined(TARGET_BLUE)

unsigned int io_seproxyhal_touch_exit(const bagl_element_t *e) {
    // Go back to the dashboard
    os_sched_exit(0);
    return 0; // do not redraw the widget
}

#if defined(TARGET_NANOS)
unsigned int ui_address_nanos_button(unsigned int button_mask, unsigned int button_mask_counter) {
    switch(button_mask) {
        case BUTTON_EVT_RELEASED|BUTTON_LEFT: // CANCEL
			      io_seproxyhal_touch_address_cancel(NULL);
            break;

        case BUTTON_EVT_RELEASED|BUTTON_RIGHT: { // OK
			      io_seproxyhal_touch_address_ok(NULL);
			      break;
        }
    }
    return 0;
}
#endif // #if defined(TARGET_NANOS)

void io_seproxyhal_send_status(uint32_t sw) {
    G_io_apdu_buffer[0] = ((sw >> 8) & 0xff);
    G_io_apdu_buffer[1] = (sw & 0xff);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

void format_signature_out(const uint8_t* signature) {
  os_memset(G_io_apdu_buffer + 1, 0x00, 64);
  uint8_t offset = 1;
  uint8_t xoffset = 4; //point to r value
  //copy r
  uint8_t xlength = signature[xoffset-1];
  if (xlength == 33) {
    xlength = 32;
    xoffset ++;
  }
  memmove(G_io_apdu_buffer+offset+32-xlength,  signature+xoffset, xlength);
  offset += 32;
  xoffset += xlength +2; //move over rvalue and TagLEn
  //copy s value
  xlength = signature[xoffset-1];
  if (xlength == 33) {
    xlength = 32;
    xoffset ++;
  }
  memmove(G_io_apdu_buffer+offset+32-xlength, signature+xoffset, xlength);
}

#if defined(TARGET_BLUE)
void ui_approval_blue_init(void) {
  UX_DISPLAY(ui_approval_blue, ui_approval_blue_prepro);
}

void ui_approval_transaction_blue_init(void) {
  ui_approval_blue_ok = (bagl_element_callback_t) io_seproxyhal_touch_tx_ok;
  ui_approval_blue_cancel = (bagl_element_callback_t) io_seproxyhal_touch_tx_cancel;
  G_ui_approval_blue_state = APPROVAL_TRANSACTION;
  ui_approval_blue_values[0] = strings.common.fullAmount;
  ui_approval_blue_values[1] = strings.common.fullAddress;
  ui_approval_blue_values[2] = strings.common.maxFee;
  ui_approval_blue_init();
}

void ui_approval_message_sign_blue_init(void) {
  ui_approval_blue_ok = (bagl_element_callback_t) io_seproxyhal_touch_signMessage_ok;
  ui_approval_blue_cancel = (bagl_element_callback_t) io_seproxyhal_touch_signMessage_cancel;
  G_ui_approval_blue_state = APPROVAL_MESSAGE;
  ui_approval_blue_values[0] = strings.common.fullAmount;
  ui_approval_blue_values[1] = NULL;
  ui_approval_blue_values[2] = NULL;
  ui_approval_blue_init();
}

#elif defined(TARGET_NANOS)
unsigned int ui_approval_nanos_button(unsigned int button_mask, unsigned int button_mask_counter) {
    switch(button_mask) {
        case BUTTON_EVT_RELEASED|BUTTON_LEFT:
            io_seproxyhal_touch_tx_cancel(NULL);
            break;

        case BUTTON_EVT_RELEASED|BUTTON_RIGHT: {
			      io_seproxyhal_touch_tx_ok(NULL);
            break;
        }
    }
    return 0;
}


unsigned int ui_approval_signMessage_nanos_button(unsigned int button_mask, unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_signMessage_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: {
        io_seproxyhal_touch_signMessage_ok(NULL);
        break;
    }
    }
    return 0;
}

unsigned int ui_data_selector_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter) {
   switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_data_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: {
        io_seproxyhal_touch_data_ok(NULL);
        break;
    }
    }
    return 0;
}

unsigned int ui_data_parameter_nanos_button(unsigned int button_mask,
                                     unsigned int button_mask_counter) {
   switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_data_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT: {
        io_seproxyhal_touch_data_ok(NULL);
        break;
    }
    }
    return 0;
}

#endif // #if defined(TARGET_NANOS)

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

tokenDefinition_t* getKnownToken(uint8_t *contractAddress) {
    tokenDefinition_t *currentToken = NULL;
#ifdef HAVE_TOKENS_LIST
    uint32_t numTokens = 0;
    uint32_t i;
    switch(chainConfig->kind) {
        case CHAIN_KIND_AKROMA:
            numTokens = NUM_TOKENS_AKROMA;
            break;
        case CHAIN_KIND_ETHEREUM:
            numTokens = NUM_TOKENS_ETHEREUM;
            break;
        case CHAIN_KIND_ETHEREUM_CLASSIC:
            numTokens = NUM_TOKENS_ETHEREUM_CLASSIC;
            break;
        case CHAIN_KIND_PIRL:
            numTokens = NUM_TOKENS_PIRL;
            break;
        case CHAIN_KIND_POA:
            numTokens = NUM_TOKENS_POA;
            break;
        case CHAIN_KIND_ARTIS_SIGMA1:
            numTokens = NUM_TOKENS_ARTIS_SIGMA1;
            break;
        case CHAIN_KIND_ARTIS_TAU1:
            numTokens = NUM_TOKENS_ARTIS_TAU1;
            break;
        case CHAIN_KIND_RSK:
            numTokens = NUM_TOKENS_RSK;
            break;
        case CHAIN_KIND_EXPANSE:
            numTokens = NUM_TOKENS_EXPANSE;
            break;
        case CHAIN_KIND_UBIQ:
            numTokens = NUM_TOKENS_UBIQ;
            break;
        case CHAIN_KIND_WANCHAIN:
            numTokens = NUM_TOKENS_WANCHAIN;
            break;
        case CHAIN_KIND_KUSD:
            numTokens = NUM_TOKENS_KUSD;
            break;
        case CHAIN_KIND_MUSICOIN:
            numTokens = NUM_TOKENS_MUSICOIN;
            break;
        case CHAIN_KIND_CALLISTO:
            numTokens = NUM_TOKENS_CALLISTO;
            break;
        case CHAIN_KIND_ETHERSOCIAL:
            numTokens = NUM_TOKENS_ETHERSOCIAL;
            break;
        case CHAIN_KIND_ELLAISM:
            numTokens = NUM_TOKENS_ELLAISM;
            break;
        case CHAIN_KIND_ETHER1:
            numTokens = NUM_TOKENS_ETHER1;
            break;
        case CHAIN_KIND_ETHERGEM:
            numTokens = NUM_TOKENS_ETHERGEM;
            break;
        case CHAIN_KIND_ATHEIOS:
            numTokens = NUM_TOKENS_ATHEIOS;
            break;
        case CHAIN_KIND_GOCHAIN:
            numTokens = NUM_TOKENS_GOCHAIN;
            break;
        case CHAIN_KIND_MIX:
            numTokens = NUM_TOKENS_MIX;
            break;
        case CHAIN_KIND_REOSC:
            numTokens = NUM_TOKENS_REOSC;
            break;
        case CHAIN_KIND_HPB:
            numTokens = NUM_TOKENS_HPB;
            break;
        case CHAIN_KIND_TOMOCHAIN:
            numTokens = NUM_TOKENS_TOMOCHAIN;
            break;
        case CHAIN_KIND_TOBALABA:
            numTokens = NUM_TOKENS_TOBALABA;
            break;
        case CHAIN_KIND_DEXON:
            numTokens = NUM_TOKENS_DEXON;
            break;
        case CHAIN_KIND_VOLTA:
            numTokens = NUM_TOKENS_VOLTA;
            break;
        case CHAIN_KIND_EWC:
            numTokens = NUM_TOKENS_EWC;
            break;
        case CHAIN_KIND_WEBCHAIN:
            numTokens = NUM_TOKENS_WEBCHAIN;
            break;
        case CHAIN_KIND_THUNDERCORE:
            numTokens = NUM_TOKENS_THUNDERCORE;
            break;
        case CHAIN_KIND_WETHIO:
            numTokens = NUM_TOKENS_WETHIO;
            break;            
    }
    for (i=0; i<numTokens; i++) {
        switch(chainConfig->kind) {
            case CHAIN_KIND_AKROMA:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_AKROMA[i]);
                break;
            case CHAIN_KIND_ETHEREUM:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ETHEREUM[i]);
                break;
            case CHAIN_KIND_ETHEREUM_CLASSIC:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ETHEREUM_CLASSIC[i]);
                break;
            case CHAIN_KIND_PIRL:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_PIRL[i]);
                break;
            case CHAIN_KIND_POA:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_POA[i]);
                break;
            case CHAIN_KIND_ARTIS_SIGMA1:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ARTIS_SIGMA1[i]);
                break;
            case CHAIN_KIND_ARTIS_TAU1:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ARTIS_TAU1[i]);
                break;
            case CHAIN_KIND_RSK:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_RSK[i]);
                break;
            case CHAIN_KIND_EXPANSE:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_EXPANSE[i]);
                break;
            case CHAIN_KIND_UBIQ:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_UBIQ[i]);
                break;
            case CHAIN_KIND_WANCHAIN:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_WANCHAIN[i]);
                break;
            case CHAIN_KIND_KUSD:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_KUSD[i]);
                break;
            case CHAIN_KIND_MUSICOIN:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_MUSICOIN[i]);
                break;
            case CHAIN_KIND_CALLISTO:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_CALLISTO[i]);
                break;
            case CHAIN_KIND_ETHERSOCIAL:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ETHERSOCIAL[i]);
                break;
            case CHAIN_KIND_ELLAISM:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ELLAISM[i]);
                break;
            case CHAIN_KIND_ETHER1:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ETHER1[i]);
                break;
            case CHAIN_KIND_ETHERGEM:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ETHERGEM[i]);
                break;
            case CHAIN_KIND_ATHEIOS:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_ATHEIOS[i]);
                break;
            case CHAIN_KIND_GOCHAIN:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_GOCHAIN[i]);
                break;
            case CHAIN_KIND_MIX:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_MIX[i]);
                break;
            case CHAIN_KIND_REOSC:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_REOSC[i]);
                break;
            case CHAIN_KIND_HPB:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_HPB[i]);
                break;
            case CHAIN_KIND_TOMOCHAIN:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_TOMOCHAIN[i]);
                break;
            case CHAIN_KIND_TOBALABA:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_TOBALABA[i]);
                break;
            case CHAIN_KIND_DEXON:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_DEXON[i]);
                break;
            case CHAIN_KIND_VOLTA:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_VOLTA[i]);
                break;
            case CHAIN_KIND_EWC:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_EWC[i]);
                break;
            case CHAIN_KIND_WEBCHAIN:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_WEBCHAIN[i]);
                break;
            case CHAIN_KIND_THUNDERCORE:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_THUNDERCORE[i]);
                break;
            case CHAIN_KIND_WETHIO:
                currentToken = (tokenDefinition_t *)PIC(&TOKENS_WETHIO[i]);
                break
        }
        if (os_memcmp(currentToken->address, tmpContent.txContent.destination, 20) == 0) {
            return currentToken;
        }
    }
#endif
    for(size_t i=0; i<MAX_TOKEN; i++){
      currentToken = &tmpCtx.transactionContext.tokens[i];
      if (tmpCtx.transactionContext.tokenSet[i] && (os_memcmp(currentToken->address, contractAddress, 20) == 0)) {
        PRINTF("Token found at index %d\n", i);
        return currentToken;
      }
    }

    return NULL;
}

void handleApdu(unsigned int *flags, unsigned int *tx) {
  unsigned short sw = 0;

  BEGIN_TRY {
    TRY {

#ifdef HAVE_STARKWARE

      if (G_io_apdu_buffer[OFFSET_CLA] == STARKWARE_CLA) {
        switch(G_io_apdu_buffer[OFFSET_INS]) {
          case STARKWARE_INS_GET_PUBLIC_KEY:
            handleStarkwareGetPublicKey(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
            break;
          case STARKWARE_INS_SIGN_MESSAGE:
            handleStarkwareSignMessage(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
            break;
          case STARKWARE_INS_PROVIDE_QUANTUM:
            handleStarkwareProvideQuantum(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
            break;
          default:
            THROW(0x6D00);
            break;
        }
        CLOSE_TRY;
        return;        
      }

#endif      

      if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
        THROW(0x6E00);
      }

      switch (G_io_apdu_buffer[OFFSET_INS]) {
        case INS_GET_PUBLIC_KEY:
          os_memset(tmpCtx.transactionContext.tokenSet, 0, MAX_TOKEN);
          handleGetPublicKey(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

        case INS_PROVIDE_ERC20_TOKEN_INFORMATION:
          handleProvideErc20TokenInformation(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

        case INS_SIGN:
          handleSign(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

        case INS_GET_APP_CONFIGURATION:
          handleGetAppConfiguration(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

        case INS_SIGN_PERSONAL_MESSAGE:
          os_memset(tmpCtx.transactionContext.tokenSet, 0, MAX_TOKEN);
          handleSignPersonalMessage(G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2], G_io_apdu_buffer + OFFSET_CDATA, G_io_apdu_buffer[OFFSET_LC], flags, tx);
          break;

#if 0
        case 0xFF: // return to dashboard
          goto return_to_dashboard;
#endif

        default:
          THROW(0x6D00);
          break;
      }
    }
    CATCH(EXCEPTION_IO_RESET) {
      THROW(EXCEPTION_IO_RESET);
    }
    CATCH_OTHER(e) {
      switch (e & 0xF000) {
        case 0x6000:
          // Wipe the transaction context and report the exception
          sw = e;
          reset_app_context();
          break;
        case 0x9000:
          // All is well
          sw = e;
          break;
        default:
          // Internal error
          sw = 0x6800 | (e & 0x7FF);
          reset_app_context();
          break;
        }
        // Unexpected exception => report
        G_io_apdu_buffer[*tx] = sw >> 8;
        G_io_apdu_buffer[*tx + 1] = sw;
        *tx += 2;
      }
      FINALLY {
      }
  }
  END_TRY;
}

void sample_main(void) {
    unsigned int rx = 0;
    unsigned int tx = 0;
    unsigned int flags = 0;

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0; // ensure no race in catch_other if io_exchange throws
                        // an error
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    THROW(0x6982);
                }
                PRINTF("New APDU received:\n%.*H\n", rx, G_io_apdu_buffer);

                handleApdu(&flags, &tx);
            }
            CATCH(EXCEPTION_IO_RESET) {
              THROW(EXCEPTION_IO_RESET);
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                case 0x6000:
                    // Wipe the transaction context and report the exception
                    sw = e;
                    reset_app_context();
                    break;
                case 0x9000:
                    // All is well
                    sw = e;
                    break;
                default:
                    // Internal error
                    sw = 0x6800 | (e & 0x7FF);
                    reset_app_context();
                    break;
                }
                if (e != 0x9000) {
                    flags &= ~IO_ASYNCH_REPLY;
                }
                // Unexpected exception => report
                G_io_apdu_buffer[tx] = sw >> 8;
                G_io_apdu_buffer[tx + 1] = sw;
                tx += 2;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

//return_to_dashboard:
    return;
}

// override point, but nothing more to do
void io_seproxyhal_display(const bagl_element_t *element) {
  io_seproxyhal_display_default((bagl_element_t *)element);
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
    		UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
    		break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID && !(U4BE(G_io_seproxyhal_spi_buffer, 3) & SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
         THROW(EXCEPTION_IO_RESET);
        }
        // no break is intentional
    default:
        UX_DEFAULT_EVENT();
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;

#if 0
    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer,
        {
        });
        break;
#endif        
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

void app_exit(void) {

    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {

        }
    }
    END_TRY_L(exit);
}

chain_config_t const C_chain_config = {
  .coinName = CHAINID_COINNAME " ",
  .chainId = CHAIN_ID,
  .kind = CHAIN_KIND,
#ifdef TARGET_BLUE
  .color_header = COLOR_APP,
  .color_dashboard = COLOR_APP_LIGHT,
  .header_text = CHAINID_UPCASE,
#endif // TARGET_BLUE
};

__attribute__((section(".boot"))) int main(int arg0) {
#ifdef USE_LIB_ETHEREUM
    chain_config_t local_chainConfig;
    os_memmove(&local_chainConfig, &C_chain_config, sizeof(chain_config_t));
    unsigned int libcall_params[3];
    unsigned char coinName[sizeof(CHAINID_COINNAME)];
    strcpy(coinName, CHAINID_COINNAME);
#ifdef TARGET_BLUE
    unsigned char coinNameUP[sizeof(CHAINID_UPCASE)];
    strcpy(coinNameUP, CHAINID_UPCASE);
    local_chainConfig.header_text = coinNameUP;
#endif // TARGET_BLUE
    local_chainConfig.coinName = coinName;
    BEGIN_TRY {
        TRY {
            // ensure syscall will accept us
            check_api_level(CX_COMPAT_APILEVEL);
            // delegate to Ethereum app/lib
            libcall_params[0] = "Ethereum";
            libcall_params[1] = 0x100; // use the Init call, as we won't exit
            libcall_params[2] = &local_chainConfig;
            os_lib_call(&libcall_params);
        }
        FINALLY {
            app_exit();
        }
    }
    END_TRY;
#else
    // exit critical section
    __asm volatile("cpsie i");

    if (arg0) {
        if (((unsigned int *)arg0)[0] != 0x100) {
            os_lib_throw(INVALID_PARAMETER);
        }
        chainConfig = (chain_config_t *)((unsigned int *)arg0)[1];
    }
    else {
        chainConfig = (chain_config_t *)PIC(&C_chain_config);
    }

    reset_app_context();
    tmpCtx.transactionContext.currentTokenIndex = 0;

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        UX_INIT();

        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

#ifdef TARGET_NANOX
                // grab the current plane mode setting
                G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif // TARGET_NANOX

                if (N_storage.initialized != 0x01) {
                  internalStorage_t storage;
                  storage.dataAllowed = 0x00;
                  storage.contractDetails = 0x00;
                  storage.initialized = 0x01;
                  nvm_write((void*)&N_storage, (void*)&storage, sizeof(internalStorage_t));
                }
                dataAllowed = N_storage.dataAllowed;
                contractDetails = N_storage.contractDetails;

                USB_power(0);
                USB_power(1);

                ui_idle();

#ifdef HAVE_BLE
                BLE_power(0, NULL);
                BLE_power(1, "Nano X");
#endif // HAVE_BLE
    #if defined(TARGET_BLUE)
                // setup the status bar colors (remembered after wards, even more if another app does not resetup after app switch)
                UX_SET_STATUS_BAR_COLOR(0xFFFFFF, chainConfig->color_header);
    #endif // #if defined(TARGET_BLUE)

                sample_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
              // reset IO and UX before continuing
              CLOSE_TRY;
              continue;
            }
            CATCH_ALL {
              CLOSE_TRY;             
              break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }
	  app_exit();
#endif
    return 0;
}
