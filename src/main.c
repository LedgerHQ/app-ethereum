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
#include "common_utils.h"

#include "swap_lib_calls.h"
#include "handle_swap_sign_transaction.h"
#include "handle_get_printable_amount.h"
#include "handle_check_address.h"
#include "commands_712.h"
#include "challenge.h"
#include "domain_name.h"
#include "crypto_helpers.h"
#include "manage_asset_info.h"

tmpCtx_t tmpCtx;
txContext_t txContext;
tmpContent_t tmpContent;
dataContext_t dataContext;
strings_t strings;
cx_sha3_t global_sha3;

uint8_t appState;
uint16_t apdu_response_code;
bool G_called_from_swap;
bool G_swap_response_ready;
pluginType_t pluginType;

#ifdef HAVE_ETH2
uint32_t eth2WithdrawalIndex;
#include "withdrawal_index.h"
#endif

#include "ux.h"

const internalStorage_t N_storage_real;

#ifdef HAVE_NBGL
caller_app_t *caller_app = NULL;
#endif
const chain_config_t *chainConfig;

void reset_app_context() {
    // PRINTF("!!RESET_APP_CONTEXT\n");
    appState = APP_STATE_IDLE;
    G_called_from_swap = false;
    G_swap_response_ready = false;
    pluginType = OLD_INTERNAL;
#ifdef HAVE_ETH2
    eth2WithdrawalIndex = 0;
#endif
    memset((uint8_t *) &tmpCtx, 0, sizeof(tmpCtx));
    forget_known_assets();
    memset((uint8_t *) &txContext, 0, sizeof(txContext));
    memset((uint8_t *) &tmpContent, 0, sizeof(tmpContent));
}

unsigned int io_seproxyhal_send_status(uint32_t sw, uint32_t tx, bool reset, bool idle) {
    unsigned int err = 0;
    if (reset) {
        reset_app_context();
    }
    U2BE_ENCODE(G_io_apdu_buffer, tx, sw);
    tx += 2;
    err = io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    if (idle) {
        // Display back the original UX
        ui_idle();
    }
    return err;
}

const uint8_t *parseBip32(const uint8_t *dataBuffer, uint8_t *dataLength, bip32_path_t *bip32) {
    if (*dataLength < 1) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    bip32->length = *dataBuffer;

    dataBuffer++;
    (*dataLength)--;

    if (*dataLength < sizeof(uint32_t) * (bip32->length)) {
        PRINTF("Invalid data\n");
        return NULL;
    }

    if (bip32_path_read(dataBuffer, (size_t) dataLength, bip32->path, (size_t) bip32->length) ==
        false) {
        PRINTF("Invalid Path data\n");
        return NULL;
    }
    dataBuffer += bip32->length * sizeof(uint32_t);
    *dataLength -= bip32->length * sizeof(uint32_t);

    return dataBuffer;
}

void handleApdu(unsigned int *flags, unsigned int *tx) {
    unsigned short sw = 0;

    BEGIN_TRY {
        TRY {
#ifndef HAVE_LEDGER_PKI
            if ((G_io_apdu_buffer[OFFSET_CLA] == 0xB0 && (G_io_apdu_buffer[OFFSET_INS] == 0x06))) {
                // Ledger-PKI APDU not yet caught by the running OS.
                // Command code not supported
                PRINTF("Ledger-PKI not yet supported!\n");
                THROW(APDU_RESPONSE_CMD_CODE_NOT_SUPPORTED);
            }
#endif  // HAVE_LEDGER_PKI

            if (G_io_apdu_buffer[OFFSET_CLA] != CLA) {
                THROW(APDU_RESPONSE_INVALID_CLA);
            }

            switch (G_io_apdu_buffer[OFFSET_INS]) {
                case INS_GET_PUBLIC_KEY:
                    forget_known_assets();
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
                    forget_known_assets();
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
                            forget_known_assets();
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
                    forget_known_assets();
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
                    THROW(APDU_RESPONSE_INVALID_INS);
                    break;
            }
        }
        CATCH(EXCEPTION_IO_RESET) {
            THROW(EXCEPTION_IO_RESET);
        }
        CATCH_OTHER(e) {
            bool quit_now = G_called_from_swap && G_swap_response_ready;
            switch (e & 0xF000) {
                case 0x6000:
                    // Wipe the transaction context and report the exception
                    sw = e;
                    reset_app_context();
                    break;
                case APDU_RESPONSE_OK:
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

            // If we are in swap mode and have validated a TX, we send it and immediately quit
            if (quit_now) {
                if (io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, *tx) == 0) {
                    // In case of success, the apdu is sent immediately and eth exits
                    // Reaching this code means we encountered an error
                    finalize_exchange_sign_transaction(false);
                } else {
                    PRINTF("Unrecoverable\n");
                    app_exit();
                }
            }
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
                    THROW(APDU_RESPONSE_WRONG_DATA_LENGTH);
                }
                if (rx > OFFSET_LC && rx != (G_io_apdu_buffer[OFFSET_LC] + 5)) {
                    THROW(APDU_RESPONSE_WRONG_DATA_LENGTH);
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
                    case APDU_RESPONSE_OK:
                        // All is well
                        sw = e;
                        break;
                    default:
                        // Internal error
                        sw = APDU_RESPONSE_INTERNAL_ERROR | (e & 0x7FF);
                        reset_app_context();
                        break;
                }
                if (e != APDU_RESPONSE_OK) {
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
}

void init_coin_config(chain_config_t *coin_config) {
    memset(coin_config, 0, sizeof(chain_config_t));
    strcpy(coin_config->coinName, CHAINID_COINNAME);
    coin_config->chainId = CHAIN_ID;
}

__attribute__((noreturn)) void coin_main(libargs_t *args) {
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

    for (;;) {
        BEGIN_TRY {
            TRY {
                common_app_init();

                if (!N_storage.initialized) {
                    internalStorage_t storage;
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

                ui_idle();

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
    while (1)
        ;
}

__attribute__((noreturn)) void library_main(libargs_t *args) {
    chain_config_t coin_config;
    if (args->chain_config == NULL) {
        // We have been started directly by Exchange, not by a Clone. Init default chain
        init_coin_config(&coin_config);
        args->chain_config = &coin_config;
    }

    PRINTF("Inside a library \n");
    switch (args->command) {
        case CHECK_ADDRESS:
            if (handle_check_address(args->check_address, args->chain_config) != APDU_RESPONSE_OK) {
                // Failed, non recoverable
                app_exit();
            }
            break;
        case SIGN_TRANSACTION:
            if (copy_transaction_parameters(args->create_transaction, args->chain_config)) {
                // never returns
                handle_swap_sign_transaction(args->chain_config);
            } else {
                // Failed to copy, non recoverable
                app_exit();
            }
            break;
        case GET_PRINTABLE_AMOUNT:
            if (handle_get_printable_amount(args->get_printable_amount, args->chain_config) !=
                APDU_RESPONSE_OK) {
                // Failed, non recoverable
                app_exit();
            }
            break;
        default:
            break;
    }
    os_lib_end();
}

/* Eth clones do not actually contain any logic, they delegate everything to the ETH application.
 * Start Eth in lib mode with the correct chain config
 */
__attribute__((noreturn)) void clone_main(libargs_t *args) {
    PRINTF("Starting in clone_main\n");
    BEGIN_TRY {
        TRY {
            unsigned int libcall_params[5];
            chain_config_t local_chainConfig;
            init_coin_config(&local_chainConfig);

            libcall_params[0] = (unsigned int) "Ethereum";
            libcall_params[1] = 0x100;
            libcall_params[3] = (unsigned int) &local_chainConfig;

            // Clone called by Exchange, forward the request to Ethereum
            if (args != NULL) {
                if (args->id != 0x100) {
                    os_sched_exit(0);
                }
                libcall_params[2] = args->command;
                libcall_params[4] = (unsigned int) args->get_printable_amount;
                os_lib_call((unsigned int *) &libcall_params);
                // Ethereum fulfilled the request and returned to us. We return to Exchange.
                os_lib_end();
            } else {
                // Clone called from Dashboard, start Ethereum
                libcall_params[2] = RUN_APPLICATION;
// On Stax, forward our icon to Ethereum
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
                libcall_params[4] = 0;
#endif  // HAVE_NBGL
                os_lib_call((unsigned int *) &libcall_params);
                // Ethereum should not return to us
                app_exit();
            }
        }
        FINALLY {
        }
    }
    END_TRY;

    // os_lib_call will raise if Ethereum application is not installed. Do not try to recover.
    app_exit();
    while (1)
        ;
}

int ethereum_main(libargs_t *args) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    if (args == NULL) {
        // called from dashboard as standalone eth app
        coin_main(NULL);
        return 0;
    }

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
    return 0;
}

__attribute__((section(".boot"))) int main(int arg0) {
#ifdef USE_LIB_ETHEREUM
    clone_main((libargs_t *) arg0);
#else
    return ethereum_main((libargs_t *) arg0);
#endif
}
