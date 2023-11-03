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
#include "common_ui.h"

#include "os_io_seproxyhal.h"

#include "glyphs.h"
#include "utils.h"

#include "swap_lib_calls.h"
#include "handle_swap_sign_transaction.h"
#include "handle_get_printable_amount.h"
#include "handle_check_address.h"
#include "commands_712.h"
#include "challenge.h"
#include "domain_name.h"

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
cx_sha3_t global_sha3;

uint8_t appState;
uint16_t apdu_response_code;
bool G_called_from_swap;
pluginType_t pluginType;
#ifdef HAVE_STARKWARE
bool quantumSet;
#endif

#ifdef HAVE_ETH2
uint32_t eth2WithdrawalIndex;
#include "withdrawal_index.h"
#endif

#include "ux.h"
ux_state_t G_ux;
bolos_ux_params_t G_ux_params;

const internalStorage_t N_storage_real;

#ifdef HAVE_NBGL
caller_app_t *caller_app = NULL;
#endif
chain_config_t *chainConfig = NULL;

void reset_app_context() {
    // PRINTF("!!RESET_APP_CONTEXT\n");
    appState = APP_STATE_IDLE;
    G_called_from_swap = false;
    pluginType = OLD_INTERNAL;
#ifdef HAVE_STARKWARE
    quantumSet = false;
#endif
#ifdef HAVE_ETH2
    eth2WithdrawalIndex = 0;
#endif
    memset((uint8_t *) &tmpCtx, 0, sizeof(tmpCtx));
    memset((uint8_t *) &txContext, 0, sizeof(txContext));
    memset((uint8_t *) &tmpContent, 0, sizeof(tmpContent));
}

void io_seproxyhal_send_status(uint32_t sw) {
    G_io_apdu_buffer[0] = ((sw >> 8) & 0xff);
    G_io_apdu_buffer[1] = (sw & 0xff);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

void format_signature_out(const uint8_t *signature) {
    memset(G_io_apdu_buffer + 1, 0x00, 64);
    uint8_t offset = 1;
    uint8_t xoffset = 4;  // point to r value
    // copy r
    uint8_t xlength = signature[xoffset - 1];
    if (xlength == 33) {
        xlength = 32;
        xoffset++;
    }
    memmove(G_io_apdu_buffer + offset + 32 - xlength, signature + xoffset, xlength);
    offset += 32;
    xoffset += xlength + 2;  // move over rvalue and TagLEn
    // copy s value
    xlength = signature[xoffset - 1];
    if (xlength == 33) {
        xlength = 32;
        xoffset++;
    }
    memmove(G_io_apdu_buffer + offset + 32 - xlength, signature + xoffset, xlength);
}

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
                return 0;  // nothing received from the master so far (it's a tx
                           // transaction)
            } else {
                return io_seproxyhal_spi_recv(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), 0);
            }

        default:
            THROW(INVALID_PARAMETER);
    }
    return 0;
}

extraInfo_t *getKnownToken(uint8_t *contractAddress) {
    union extraInfo_t *currentItem = NULL;
    // Works for ERC-20 & NFT tokens since both structs in the union have the
    // contract address aligned
    for (uint8_t i = 0; i < MAX_ITEMS; i++) {
        currentItem = (union extraInfo_t *) &tmpCtx.transactionContext.extraInfo[i].token;
        if (tmpCtx.transactionContext.tokenSet[i] &&
            (memcmp(currentItem->token.address, contractAddress, ADDRESS_LENGTH) == 0)) {
            PRINTF("Token found at index %d\n", i);
            return currentItem;
        }
    }

    return NULL;
}

#ifndef HAVE_WALLET_ID_SDK

unsigned int const U_os_perso_seed_cookie[] = {
    0xda7aba5e,
    0xc1a551c5,
};

void handleGetWalletId(volatile unsigned int *tx) {
    unsigned char t[64];
    cx_ecfp_256_private_key_t priv;
    cx_ecfp_256_public_key_t pub;
    // seed => priv key
    os_perso_derive_node_bip32(CX_CURVE_256K1, U_os_perso_seed_cookie, 2, t, NULL);
    // priv key => pubkey
    cx_ecdsa_init_private_key(CX_CURVE_256K1, t, 32, &priv);
    cx_ecfp_generate_pair(CX_CURVE_256K1, &pub, &priv, 1);
    // pubkey -> sha512
    cx_hash_sha512(pub.W, sizeof(pub.W), t, sizeof(t));
    // ! cookie !
    memmove(G_io_apdu_buffer, t, 64);
    *tx = 64;
    THROW(0x9000);
}

#endif  // HAVE_WALLET_ID_SDK

const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    if (*dataLength < 1) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    bip32->length = *dataBuffer;

    if (bip32->length < 0x1 || bip32->length > MAX_BIP32_PATH) {
        PRINTF("Invalid bip32\n");
        return NULL;
    }

    dataBuffer++;
    (*dataLength)--;

    if (*dataLength < sizeof(uint32_t) * (bip32->length)) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    for (uint8_t i = 0; i < bip32->length; i++) {
        bip32->path[i] = U4BE(dataBuffer, 0);
        dataBuffer += sizeof(uint32_t);
        *dataLength -= sizeof(uint32_t);
    }

    return dataBuffer;
}

void handleApdu(unsigned int *flags, unsigned int *tx) {
    unsigned short sw = 0;

    BEGIN_TRY {
        TRY {
#ifndef HAVE_WALLET_ID_SDK

            if ((G_io_apdu_buffer[OFFSET_CLA] == COMMON_CLA) &&
                (G_io_apdu_buffer[OFFSET_INS] == COMMON_INS_GET_WALLET_ID)) {
                handleGetWalletId(tx);
                return;
            }

#endif  // HAVE_WALLET_ID_SDK

#ifdef HAVE_STARKWARE

            if (G_io_apdu_buffer[OFFSET_CLA] == STARKWARE_CLA) {
                switch (G_io_apdu_buffer[OFFSET_INS]) {
                    case STARKWARE_INS_GET_PUBLIC_KEY:
                        handleStarkwareGetPublicKey(G_io_apdu_buffer[OFFSET_P1],
                                                    G_io_apdu_buffer[OFFSET_P2],
                                                    G_io_apdu_buffer + OFFSET_CDATA,
                                                    G_io_apdu_buffer[OFFSET_LC],
                                                    flags,
                                                    tx);
                        break;
                    case STARKWARE_INS_SIGN_MESSAGE:
                        handleStarkwareSignMessage(G_io_apdu_buffer[OFFSET_P1],
                                                   G_io_apdu_buffer[OFFSET_P2],
                                                   G_io_apdu_buffer + OFFSET_CDATA,
                                                   G_io_apdu_buffer[OFFSET_LC],
                                                   flags,
                                                   tx);
                        break;
                    case STARKWARE_INS_PROVIDE_QUANTUM:
                        handleStarkwareProvideQuantum(G_io_apdu_buffer[OFFSET_P1],
                                                      G_io_apdu_buffer[OFFSET_P2],
                                                      G_io_apdu_buffer + OFFSET_CDATA,
                                                      G_io_apdu_buffer[OFFSET_LC],
                                                      flags,
                                                      tx);
                        break;
                    case STARKWARE_INS_UNSAFE_SIGN:
                        handleStarkwareUnsafeSign(G_io_apdu_buffer[OFFSET_P1],
                                                  G_io_apdu_buffer[OFFSET_P2],
                                                  G_io_apdu_buffer + OFFSET_CDATA,
                                                  G_io_apdu_buffer[OFFSET_LC],
                                                  flags,
                                                  tx);
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
                    memset(tmpCtx.transactionContext.tokenSet, 0, MAX_ITEMS);
                    handleGetPublicKey(G_io_apdu_buffer[OFFSET_P1],
                                       G_io_apdu_buffer[OFFSET_P2],
                                       G_io_apdu_buffer + OFFSET_CDATA,
                                       G_io_apdu_buffer[OFFSET_LC],
                                       flags,
                                       tx);
                    break;

                case INS_PROVIDE_ERC20_TOKEN_INFORMATION:
                    handleProvideErc20TokenInformation(G_io_apdu_buffer[OFFSET_P1],
                                                       G_io_apdu_buffer[OFFSET_P2],
                                                       G_io_apdu_buffer + OFFSET_CDATA,
                                                       G_io_apdu_buffer[OFFSET_LC],
                                                       flags,
                                                       tx);
                    break;

#ifdef HAVE_NFT_SUPPORT
                case INS_PROVIDE_NFT_INFORMATION:
                    handleProvideNFTInformation(G_io_apdu_buffer[OFFSET_P1],
                                                G_io_apdu_buffer[OFFSET_P2],
                                                G_io_apdu_buffer + OFFSET_CDATA,
                                                G_io_apdu_buffer[OFFSET_LC],
                                                flags,
                                                tx);
                    break;
#endif  // HAVE_NFT_SUPPORT

                case INS_SET_EXTERNAL_PLUGIN:
                    handleSetExternalPlugin(G_io_apdu_buffer[OFFSET_P1],
                                            G_io_apdu_buffer[OFFSET_P2],
                                            G_io_apdu_buffer + OFFSET_CDATA,
                                            G_io_apdu_buffer[OFFSET_LC],
                                            flags,
                                            tx);
                    break;

                case INS_SET_PLUGIN:
                    handleSetPlugin(G_io_apdu_buffer[OFFSET_P1],
                                    G_io_apdu_buffer[OFFSET_P2],
                                    G_io_apdu_buffer + OFFSET_CDATA,
                                    G_io_apdu_buffer[OFFSET_LC],
                                    flags,
                                    tx);
                    break;

                case INS_PERFORM_PRIVACY_OPERATION:
                    handlePerformPrivacyOperation(G_io_apdu_buffer[OFFSET_P1],
                                                  G_io_apdu_buffer[OFFSET_P2],
                                                  G_io_apdu_buffer + OFFSET_CDATA,
                                                  G_io_apdu_buffer[OFFSET_LC],
                                                  flags,
                                                  tx);
                    break;

                case INS_SIGN:
                    handleSign(G_io_apdu_buffer[OFFSET_P1],
                               G_io_apdu_buffer[OFFSET_P2],
                               G_io_apdu_buffer + OFFSET_CDATA,
                               G_io_apdu_buffer[OFFSET_LC],
                               flags,
                               tx);
                    break;

                case INS_GET_APP_CONFIGURATION:
                    handleGetAppConfiguration(G_io_apdu_buffer[OFFSET_P1],
                                              G_io_apdu_buffer[OFFSET_P2],
                                              G_io_apdu_buffer + OFFSET_CDATA,
                                              G_io_apdu_buffer[OFFSET_LC],
                                              flags,
                                              tx);
                    break;

                case INS_SIGN_PERSONAL_MESSAGE:
                    memset(tmpCtx.transactionContext.tokenSet, 0, MAX_ITEMS);
                    *flags |= IO_ASYNCH_REPLY;
                    if (!handleSignPersonalMessage(G_io_apdu_buffer[OFFSET_P1],
                                                   G_io_apdu_buffer[OFFSET_P2],
                                                   G_io_apdu_buffer + OFFSET_CDATA,
                                                   G_io_apdu_buffer[OFFSET_LC])) {
                        reset_app_context();
                    }
                    break;

                case INS_SIGN_EIP_712_MESSAGE:
                    switch (G_io_apdu_buffer[OFFSET_P2]) {
                        case P2_EIP712_LEGACY_IMPLEM:
                            memset(tmpCtx.transactionContext.tokenSet, 0, MAX_ITEMS);
                            handleSignEIP712Message_v0(G_io_apdu_buffer[OFFSET_P1],
                                                       G_io_apdu_buffer[OFFSET_P2],
                                                       G_io_apdu_buffer + OFFSET_CDATA,
                                                       G_io_apdu_buffer[OFFSET_LC],
                                                       flags,
                                                       tx);
                            break;
#ifdef HAVE_EIP712_FULL_SUPPORT
                        case P2_EIP712_FULL_IMPLEM:
                            *flags |= IO_ASYNCH_REPLY;
                            handle_eip712_sign(G_io_apdu_buffer);
                            break;
#endif  // HAVE_EIP712_FULL_SUPPORT
                        default:
                            THROW(APDU_RESPONSE_INVALID_P1_P2);
                    }
                    break;

#ifdef HAVE_ETH2

                case INS_GET_ETH2_PUBLIC_KEY:
                    memset(tmpCtx.transactionContext.tokenSet, 0, MAX_ITEMS);
                    handleGetEth2PublicKey(G_io_apdu_buffer[OFFSET_P1],
                                           G_io_apdu_buffer[OFFSET_P2],
                                           G_io_apdu_buffer + OFFSET_CDATA,
                                           G_io_apdu_buffer[OFFSET_LC],
                                           flags,
                                           tx);
                    break;

                case INS_SET_ETH2_WITHDRAWAL_INDEX:
                    handleSetEth2WithdrawalIndex(G_io_apdu_buffer[OFFSET_P1],
                                                 G_io_apdu_buffer[OFFSET_P2],
                                                 G_io_apdu_buffer + OFFSET_CDATA,
                                                 G_io_apdu_buffer[OFFSET_LC],
                                                 flags,
                                                 tx);
                    break;

#endif

#ifdef HAVE_EIP712_FULL_SUPPORT
                case INS_EIP712_STRUCT_DEF:
                    *flags |= IO_ASYNCH_REPLY;
                    handle_eip712_struct_def(G_io_apdu_buffer);
                    break;

                case INS_EIP712_STRUCT_IMPL:
                    *flags |= IO_ASYNCH_REPLY;
                    handle_eip712_struct_impl(G_io_apdu_buffer);
                    break;

                case INS_EIP712_FILTERING:
                    *flags |= IO_ASYNCH_REPLY;
                    handle_eip712_filtering(G_io_apdu_buffer);
                    break;
#endif  // HAVE_EIP712_FULL_SUPPORT

#ifdef HAVE_DOMAIN_NAME
                case INS_ENS_GET_CHALLENGE:
                    handle_get_challenge();
                    break;

                case INS_ENS_PROVIDE_INFO:
                    handle_provide_domain_name(G_io_apdu_buffer[OFFSET_P1],
                                               G_io_apdu_buffer[OFFSET_P2],
                                               G_io_apdu_buffer + OFFSET_CDATA,
                                               G_io_apdu_buffer[OFFSET_LC]);
                    break;
#endif  // HAVE_DOMAIN_NAME

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

void app_main(void) {
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
                tx = 0;  // ensure no race in catch_other if io_exchange throws
                         // an error
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    THROW(ERR_APDU_EMPTY);
                }
                if (rx > OFFSET_LC && rx != (G_io_apdu_buffer[OFFSET_LC] + 5)) {
                    THROW(ERR_APDU_SIZE_MISMATCH);
                }

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

    // return_to_dashboard:
    return;
}

// override point, but nothing more to do
#ifdef HAVE_BAGL
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *) element);
}
#endif

unsigned char io_event(__attribute__((unused)) unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
        case SEPROXYHAL_TAG_FINGER_EVENT:
            UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
            break;
#ifdef HAVE_BAGL
        case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
            UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
            break;
#endif  // HAVE_BAGL

        case SEPROXYHAL_TAG_STATUS_EVENT:
            if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
                !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
                  SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
                THROW(EXCEPTION_IO_RESET);
            }
            __attribute__((fallthrough));
        default:
            UX_DEFAULT_EVENT();
            break;

        case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
#ifdef HAVE_BAGL
            UX_DISPLAYED_EVENT({});
#endif  // HAVE_BAGL
#ifdef HAVE_NBGL
            UX_DEFAULT_EVENT();
#endif  // HAVE_NBGL
            break;

        case SEPROXYHAL_TAG_TICKER_EVENT:
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
            break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

void app_exit() {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

void init_coin_config(chain_config_t *coin_config) {
    memset(coin_config, 0, sizeof(chain_config_t));
    strcpy(coin_config->coinName, CHAINID_COINNAME);
    coin_config->chainId = CHAIN_ID;
}

void coin_main(libargs_t *args) {
    chain_config_t config;
    if (args) {
        if (args->chain_config != NULL) {
            chainConfig = args->chain_config;
        }
#ifdef HAVE_NBGL
        if ((caller_app = args->caller_app) != NULL) {
            if (chainConfig != NULL) {
                caller_app->type = CALLER_TYPE_CLONE;
            } else {
                caller_app->type = CALLER_TYPE_PLUGIN;
            }
        }
#endif
    }
    if (chainConfig == NULL) {
        init_coin_config(&config);
        chainConfig = &config;
    }

    reset_app_context();
    tmpCtx.transactionContext.currentItemIndex = 0;

    for (;;) {
        UX_INIT();

        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

#ifdef HAVE_BLE
                // grab the current plane mode setting
                G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif  // HAVE_BLE

                if (!N_storage.initialized) {
                    internalStorage_t storage;
#ifdef HAVE_ALLOW_DATA
                    storage.dataAllowed = true;
#else
                    storage.dataAllowed = false;
#endif
                    storage.contractDetails = false;
                    storage.displayNonce = false;
#ifdef HAVE_EIP712_FULL_SUPPORT
                    storage.verbose_eip712 = false;
#endif
#ifdef HAVE_DOMAIN_NAME
                    storage.verbose_domain_name = false;
#endif
                    storage.initialized = true;
                    nvm_write((void *) &N_storage, (void *) &storage, sizeof(internalStorage_t));
                }

                USB_power(0);
                USB_power(1);

                ui_idle();

#ifdef HAVE_BLE
                BLE_power(0, NULL);
                BLE_power(1, NULL);
#endif  // HAVE_BLE

#ifdef HAVE_DOMAIN_NAME
                // to prevent it from having a fixed value at boot
                roll_challenge();
#endif  // HAVE_DOMAIN_NAME

                app_main();
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
}

static void library_main_helper(libargs_t *args) {
    check_api_level(CX_COMPAT_APILEVEL);
    PRINTF("Inside a library \n");
    switch (args->command) {
        case CHECK_ADDRESS:
            // ensure result is zero if an exception is thrown
            args->check_address->result = 0;
            args->check_address->result =
                handle_check_address(args->check_address, args->chain_config);
            break;
        case SIGN_TRANSACTION:
            if (copy_transaction_parameters(args->create_transaction, args->chain_config)) {
                // never returns
                handle_swap_sign_transaction(args->chain_config);
            }
            break;
        case GET_PRINTABLE_AMOUNT:
            // ensure result is zero if an exception is thrown (compatibility breaking, disabled
            // until LL is ready)
            // args->get_printable_amount->result = 0;
            // args->get_printable_amount->result =
            handle_get_printable_amount(args->get_printable_amount, args->chain_config);
            break;
        default:
            break;
    }
}

void library_main(libargs_t *args) {
    chain_config_t coin_config;
    if (args->chain_config == NULL) {
        init_coin_config(&coin_config);
        args->chain_config = &coin_config;
    }
    volatile bool end = false;
    /* This loop ensures that library_main_helper and os_lib_end are called
     * within a try context, even if an exception is thrown */
    while (1) {
        BEGIN_TRY {
            TRY {
                if (!end) {
                    library_main_helper(args);
                }
                os_lib_end();
            }
            FINALLY {
                end = true;
            }
        }
        END_TRY;
    }
}

__attribute__((section(".boot"))) int main(int arg0) {
#ifdef USE_LIB_ETHEREUM
    BEGIN_TRY {
        TRY {
            unsigned int libcall_params[5];
            chain_config_t local_chainConfig;
            init_coin_config(&local_chainConfig);

            PRINTF("Hello from Eth-clone\n");
            check_api_level(CX_COMPAT_APILEVEL);
            // delegate to Ethereum app/lib
            libcall_params[0] = (unsigned int) "Ethereum";
            libcall_params[1] = 0x100;
            libcall_params[2] = RUN_APPLICATION;
            libcall_params[3] = (unsigned int) &local_chainConfig;
#ifdef HAVE_NBGL
            const char app_name[] = APPNAME;
            caller_app_t capp;
            nbgl_icon_details_t icon_details;
            uint8_t bitmap[sizeof(ICONBITMAP)];

            memcpy(&icon_details, &ICONGLYPH, sizeof(ICONGLYPH));
            memcpy(&bitmap, &ICONBITMAP, sizeof(bitmap));
            icon_details.bitmap = (const uint8_t *) bitmap;
            capp.name = app_name;
            capp.icon = &icon_details;
            libcall_params[4] = (unsigned int) &capp;
#else
            libcall_params[4] = NULL;
#endif  // HAVE_NBGL

            if (arg0) {
                // call as a library
                libcall_params[2] = ((unsigned int *) arg0)[1];
                libcall_params[4] = ((unsigned int *) arg0)[3];  // library arguments
                os_lib_call((unsigned int *) &libcall_params);
                ((unsigned int *) arg0)[0] = libcall_params[1];
                os_lib_end();
            } else {
                // launch coin application
                libcall_params[1] = 0x100;  // use the Init call, as we won't exit
                os_lib_call((unsigned int *) &libcall_params);
            }
        }
        FINALLY {
        }
    }
    END_TRY;
    // no return
#else
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    if (!arg0) {
        // called from dashboard as standalone eth app
        coin_main(NULL);
        return 0;
    }

    libargs_t *args = (libargs_t *) arg0;
    if (args->id != 0x100) {
        app_exit();
        return 0;
    }
    switch (args->command) {
        case RUN_APPLICATION:
            // called as ethereum from altcoin or plugin
            coin_main(args);
            break;
        default:
            // called as ethereum or altcoin library
            library_main(args);
    }
#endif
    return 0;
}
