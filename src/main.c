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
#include "io.h"

#include "parser.h"
#include "glyphs.h"
#include "common_utils.h"

#include "eth_swap_utils.h"
#include "handle_swap_sign_transaction.h"
#include "handle_get_printable_amount.h"
#include "handle_check_address.h"
#include "commands_712.h"
#include "challenge.h"
#include "trusted_name.h"
#include "crypto_helpers.h"
#include "manage_asset_info.h"
#include "network_dynamic.h"
#ifdef HAVE_DYN_MEM_ALLOC
#include "mem.h"
#endif
#include "cmd_enum_value.h"
#include "cmd_tx_info.h"
#include "cmd_field.h"

tmpCtx_t tmpCtx;
txContext_t txContext;
tmpContent_t tmpContent;
dataContext_t dataContext;
strings_t strings;
cx_sha3_t global_sha3;

uint8_t appState;
uint16_t apdu_response_code;
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
#ifdef HAVE_GENERIC_TX_PARSER
    if (txContext.store_calldata) {
        gcs_cleanup();
    }
#endif
    memset((uint8_t *) &txContext, 0, sizeof(txContext));
    memset((uint8_t *) &tmpContent, 0, sizeof(tmpContent));
}

uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    uint16_t err = 0;
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

static uint16_t handleApdu(command_t *cmd, uint32_t *flags, uint32_t *tx) {
    uint16_t sw = APDU_NO_RESPONSE;

#ifndef HAVE_LEDGER_PKI
    if ((cmd->cla == 0xB0) && (cmd->ins == 0x06)) {
        // Ledger-PKI APDU not yet caught by the running OS.
        // Command code not supported
        PRINTF("Ledger-PKI not yet supported!\n");
        return APDU_RESPONSE_CMD_CODE_NOT_SUPPORTED;
    }
#endif  // HAVE_LEDGER_PKI

    if (cmd->cla != CLA) {
        return APDU_RESPONSE_INVALID_CLA;
    }

    switch (cmd->ins) {
        case INS_PROVIDE_NETWORK_CONFIGURATION:
            sw = handleNetworkConfiguration(cmd->p1, cmd->p2, cmd->data, cmd->lc, tx);
            break;
        case INS_GET_PUBLIC_KEY:
            forget_known_assets();
            sw = handleGetPublicKey(cmd->p1, cmd->p2, cmd->data, cmd->lc, flags, tx);
            break;

        case INS_PROVIDE_ERC20_TOKEN_INFORMATION:
            sw = handleProvideErc20TokenInformation(cmd->data, cmd->lc, tx);
            break;

#ifdef HAVE_NFT_SUPPORT
        case INS_PROVIDE_NFT_INFORMATION:
            sw = handleProvideNFTInformation(cmd->data, cmd->lc, tx);
            break;
#endif  // HAVE_NFT_SUPPORT

        case INS_SET_EXTERNAL_PLUGIN:
            sw = handleSetExternalPlugin(cmd->data, cmd->lc);
            break;

        case INS_SET_PLUGIN:
            sw = handleSetPlugin(cmd->data, cmd->lc);
            break;

        case INS_PERFORM_PRIVACY_OPERATION:
            sw = handlePerformPrivacyOperation(cmd->p1, cmd->p2, cmd->data, cmd->lc, flags, tx);
            break;

        case INS_SIGN:
            sw = handleSign(cmd->p1, cmd->p2, cmd->data, cmd->lc, flags);
            break;

        case INS_GET_APP_CONFIGURATION:
            sw = handleGetAppConfiguration(tx);
            break;

        case INS_SIGN_PERSONAL_MESSAGE:
            forget_known_assets();
            sw = handleSignPersonalMessage(cmd->p1, cmd->data, cmd->lc, flags);
            break;

        case INS_SIGN_EIP_712_MESSAGE:
            switch (cmd->p2) {
                case P2_EIP712_LEGACY_IMPLEM:
                    forget_known_assets();
                    sw = handleSignEIP712Message_v0(cmd->p1, cmd->data, cmd->lc, flags);
                    break;
#ifdef HAVE_EIP712_FULL_SUPPORT
                case P2_EIP712_FULL_IMPLEM:
                    sw = handle_eip712_sign(cmd->data, cmd->lc, flags);
                    break;
#endif  // HAVE_EIP712_FULL_SUPPORT
                default:
                    sw = APDU_RESPONSE_INVALID_P1_P2;
            }
            break;

#ifdef HAVE_ETH2

        case INS_GET_ETH2_PUBLIC_KEY:
            forget_known_assets();
            sw = handleGetEth2PublicKey(cmd->p1, cmd->p2, cmd->data, cmd->lc, flags, tx);
            break;

        case INS_SET_ETH2_WITHDRAWAL_INDEX:
            sw = handleSetEth2WithdrawalIndex(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;

#endif

#ifdef HAVE_EIP712_FULL_SUPPORT
        case INS_EIP712_STRUCT_DEF:
            sw = handle_eip712_struct_def(cmd->p2, cmd->data, cmd->lc);
            break;

        case INS_EIP712_STRUCT_IMPL:
            sw = handle_eip712_struct_impl(cmd->p1, cmd->p2, cmd->data, cmd->lc, flags);
            break;

        case INS_EIP712_FILTERING:
            sw = handle_eip712_filtering(cmd->p1, cmd->p2, cmd->data, cmd->lc, flags);
            break;
#endif  // HAVE_EIP712_FULL_SUPPORT

#ifdef HAVE_TRUSTED_NAME
        case INS_ENS_GET_CHALLENGE:
            sw = handle_get_challenge(tx);
            break;

        case INS_ENS_PROVIDE_INFO:
            sw = handle_provide_trusted_name(cmd->p1, cmd->data, cmd->lc);
            break;
#endif  // HAVE_TRUSTED_NAME

#ifdef HAVE_ENUM_VALUE
        case INS_PROVIDE_ENUM_VALUE:
            sw = handle_enum_value(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;
#endif  // HAVE_ENUM_VALUE

#ifdef HAVE_GENERIC_TX_PARSER
        case INS_GTP_TRANSACTION_INFO:
            sw = handle_tx_info(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;

        case INS_GTP_FIELD:
            sw = handle_field(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;
#endif  // HAVE_GENERIC_TX_PARSER

        default:
            sw = APDU_RESPONSE_INVALID_INS;
            break;
    }
    return sw;
}

void app_main(void) {
    uint32_t rx = 0;
    uint32_t tx = 0;
    uint32_t flags = 0;
    uint16_t sw = APDU_NO_RESPONSE;
    bool quit_now = false;
    command_t cmd = {0};

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        BEGIN_TRY {
            TRY {
                rx = io_exchange(CHANNEL_APDU | flags, tx);

                if (apdu_parser(&cmd, G_io_apdu_buffer, rx) == false) {
                    PRINTF("=> BAD LENGTH: %d\n", rx);
                    sw = APDU_RESPONSE_WRONG_DATA_LENGTH;
                } else {
                    PRINTF("=> CLA=%02x, INS=%02x, P1=%02x, P2=%02x, LC=%02x, CDATA=%.*h\n",
                           cmd.cla,
                           cmd.ins,
                           cmd.p1,
                           cmd.p2,
                           cmd.lc,
                           cmd.lc,
                           cmd.data);

                    tx = 0;
                    flags = 0;
                    sw = handleApdu(&cmd, &flags, &tx);
                }
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX before continuing
                CLOSE_TRY;
                app_exit();
            }
            CATCH_OTHER(e) {
                PRINTF("=> CATCH_OTHER: 0x%x\n", e);
                // Just report the exception
                sw = e;
            }
            FINALLY {
            }
        }
        END_TRY;
        if (sw == APDU_NO_RESPONSE) {
            // Nothing to report
            continue;
        }
        quit_now = G_called_from_swap && G_swap_response_ready;
        if ((sw != APDU_RESPONSE_OK) && (sw != APDU_RESPONSE_CMD_CODE_NOT_SUPPORTED)) {
            if ((sw & 0xF000) != 0x6000) {
                // Internal error
                sw = APDU_RESPONSE_INTERNAL_ERROR | (sw & 0x7FF);
            }
            reset_app_context();
            flags &= ~IO_ASYNCH_REPLY;
        }

        // Report Status Word
        U2BE_ENCODE(G_io_apdu_buffer, tx, sw);
        tx += 2;

        // If we are in swap mode and have validated a TX, we send it and immediately quit
        if (quit_now) {
            if (io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx) == 0) {
                // In case of success, the apdu is sent immediately and eth exits
                // Reaching this code means we encountered an error
                finalize_exchange_sign_transaction(false);
            } else {
                PRINTF("Unrecoverable\n");
                app_exit();
            }
        }
    }
}

static void init_coin_config(chain_config_t *coin_config) {
    memset(coin_config, 0, sizeof(chain_config_t));
    strcpy(coin_config->coinName, APP_TICKER);
    coin_config->chainId = APP_CHAIN_ID;
}

void storage_init(void) {
    internalStorage_t storage;
    if (N_storage.initialized) {
        return;
    }

    explicit_bzero(&storage, sizeof(storage));
    storage.initialized = true;
    nvm_write((void *) &N_storage, (void *) &storage, sizeof(internalStorage_t));
}

void coin_main(eth_libargs_t *args) {
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
    storage_init();
    common_app_init();

    io_init();
    ui_idle();
#ifdef HAVE_DYN_MEM_ALLOC
    mem_init();
#endif

#ifdef HAVE_TRUSTED_NAME
    // to prevent it from having a fixed value at boot
    roll_challenge();
#endif  // HAVE_TRUSTED_NAME

    app_main();
}

__attribute__((noreturn)) void library_main(eth_libargs_t *args) {
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
__attribute__((noreturn)) void clone_main(eth_libargs_t *args) {
    PRINTF("Starting in clone_main\n");
    uint32_t libcall_params[5];
    chain_config_t local_chainConfig;
    init_coin_config(&local_chainConfig);

    libcall_params[0] = (uint32_t) "Ethereum";
    libcall_params[1] = 0x100;
    libcall_params[3] = (uint32_t) &local_chainConfig;

    // Clone called by Exchange, forward the request to Ethereum
    if (args != NULL) {
        if (args->id != 0x100) {
            os_sched_exit(0);
        }
        libcall_params[2] = args->command;
        libcall_params[4] = (uint32_t) args->get_printable_amount;
        os_lib_call((uint32_t *) &libcall_params);
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
        libcall_params[4] = (uint32_t) &capp;
#else
        libcall_params[4] = 0;
#endif  // HAVE_NBGL
        os_lib_call((uint32_t *) &libcall_params);
        // Ethereum should not return to us
        app_exit();
    }

    // os_lib_call will raise if Ethereum application is not installed. Do not try to recover.
    os_sched_exit(-1);
}

int ethereum_main(eth_libargs_t *args) {
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
    clone_main((eth_libargs_t *) arg0);
#else
    return ethereum_main((eth_libargs_t *) arg0);
#endif
}
