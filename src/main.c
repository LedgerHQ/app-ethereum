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
#include "feature_sign_tx.h"  // g_tx_hash_ctx

#include "os_io_seproxyhal.h"
#include "io.h"

#include "parser.h"
#include "common_utils.h"

#include "eth_swap_utils.h"
#include "handle_swap_sign_transaction.h"
#include "handle_get_printable_amount.h"
#include "handle_check_address.h"
#include "swap_entrypoints.h"
#include "eip712_v1_commands.h"
#include "eip712_v1_context.h"  // eip712_v1_context, eip712_v1_context_deinit
#include "challenge.h"
#include "cmd_trusted_name.h"
#include "crypto_helpers.h"
#include "manage_asset_info.h"
#include "cmd_network_info.h"
#include "app_mem_utils.h"
#include "mem_utils.h"
#include "cmd_enum_value.h"
#include "cmd_map_entry.h"
#include "map_entry.h"
#include "cmd_tx_info.h"
#include "cmd_field.h"
#include "cmd_get_tx_simulation.h"
#include "cmd_get_gating.h"
#include "cmd_proxy_info.h"
#include "commands_7702.h"
#include "sign_message.h"
#include "ui_utils.h"
#include "network_info.h"
#ifdef HAVE_ADDRESS_BOOK
#include "handle_contacts.h"
#ifdef HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
#endif  // HAVE_ADDRESS_BOOK_LEDGER_ACCOUNT
#endif  // HAVE_ADDRESS_BOOK
#include "cmd_safe_account.h"
#include "tx_ctx.h"
#include "enum_value.h"
#include "proxy_info.h"

tmpCtx_t tmpCtx;
txContext_t txContext;
tmpContent_t tmpContent;
dataContext_t dataContext;
strings_t strings;

uint8_t appState;
pluginType_t pluginType;

#ifdef HAVE_ETH2
uint32_t eth2WithdrawalIndex;
#include "withdrawal_index.h"
#endif

#include "ux.h"

const internalStorage_t N_storage_real;

const caller_app_t *g_caller_app = NULL;
const chain_config_t *g_chain_config;

uint16_t io_seproxyhal_send_status(uint16_t sw, uint32_t tx, bool reset, bool idle) {
    if (reset) {
        reset_app_context();
    }
    int err = io_send_response_pointer(G_io_tx_buffer, tx, sw);
    if (idle) {
        // Display back the original UX
        ui_idle();
    }
    return (uint16_t) (err < 0 ? 1 : 0);
}

void app_main(void) {
    uint32_t tx = 0;
    uint16_t sw = SWO_NO_RESPONSE;
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
                int rx = io_recv_command();
                tx = 0;

                if (apdu_parser(&cmd, G_io_apdu_buffer, (size_t) rx) == false) {
                    PRINTF("=> BAD LENGTH: %d\n", rx);
                    sw = SWO_WRONG_DATA_LENGTH;
                } else {
                    PRINTF("=> CLA=%02x, INS=%02x (%s), P1=%02x, P2=%02x, LC=%02x, CDATA=%.*h\n",
                           cmd.cla,
                           cmd.ins,
                           INS_STR(cmd.ins),
                           cmd.p1,
                           cmd.p2,
                           cmd.lc,
                           cmd.lc,
                           cmd.data);
                    sw = handleApdu(&cmd, &tx);
                }
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX before continuing
                CLOSE_TRY;
                app_quit();
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

        if (sw == SWO_NO_RESPONSE) {
            // Nothing to report
            continue;
        }

        if ((sw != SWO_SUCCESS) && (sw != SWO_COMMAND_CODE_NOT_SUPPORTED)) {
            if ((sw & 0xF000) != 0x6000) {
                sw = SWO_NOT_SUPPORTED_ERROR_NO_INFO | (sw & 0x7FF);
            }
            // Dismiss any ongoing review before its UI buffers are freed, unless the
            // command deliberately left an error screen up. appState cannot drive this:
            // the Address Book reviews are displayed by the SDK on the app's behalf and
            // never set it.
            if (ui_error_screen_held() == false) {
                ui_idle();
            }
            reset_app_context();
        }

        io_send_response_pointer(G_io_tx_buffer, tx, sw);
    }
}

static void storage_init(void) {
    internalStorage_t storage;
    if (N_storage.initialized) {
        return;
    }

    explicit_bzero(&storage, sizeof(storage));
    storage.initialized = true;
    nvm_write((void *) &N_storage, (void *) &storage, sizeof(internalStorage_t));
}

// Common initialization for the application, both in Standalone or Library mode (Swap)
static void app_init(bool library_mode) {
    if (library_mode == false) {
        // If we are not in library mode, 1st init is the dynamic memory
        app_mem_init();
    }
    // Initialization path: do not scrub `strings`. In library (swap) mode
    // copy_transaction_parameters() has already staged the values promised by Exchange
    // there, and they must survive until finalize_parsing_helper() checks them.
    reset_app_context_ex(false);
    common_app_init();
    storage_init();
    if (library_mode == false) {
        // If we are not in library mode, we need to initialize the UX
        io_init();
        ui_idle();
    }

    // to prevent it from having a fixed value at boot
    roll_challenge();
}

void coin_main(eth_libargs_t *args) {
    chain_config_t config;
    if (args) {
        if (args->chain_config != NULL) {
            g_chain_config = args->chain_config;
        }
        if (args->caller_app != NULL) {
            if (g_chain_config != NULL) {
                args->caller_app->type = CALLER_TYPE_CLONE;
            } else {
                args->caller_app->type = CALLER_TYPE_PLUGIN;
            }
            g_caller_app = args->caller_app;
        }
    }
    if (g_chain_config == NULL) {
        init_chain_config(&config);
        g_chain_config = &config;
    }

    app_init(false);

    app_main();
}

__attribute__((noreturn)) void library_main(eth_libargs_t *args) {
    chain_config_t chain_config;
    if (args->chain_config == NULL) {
        // We have been started directly by Exchange, not by a Clone. Init default chain
        init_chain_config(&chain_config);
        args->chain_config = &chain_config;
    }

    PRINTF("Inside a library \n");
    switch (args->command) {
        case CHECK_ADDRESS:
            handle_check_address(args->check_address, args->chain_config);
            break;
        case SIGN_TRANSACTION:
            if (copy_transaction_parameters(args->create_transaction, args->chain_config)) {
                app_init(true);
                // never returns
                handle_swap_sign_transaction(args->chain_config);
            }
            break;
        case GET_PRINTABLE_AMOUNT:
            handle_get_printable_amount(args->get_printable_amount, args->chain_config);
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
    chain_config_t local_chain_config;
    init_chain_config(&local_chain_config);

    libcall_params[0] = (uint32_t) "Ethereum";
    libcall_params[1] = 0x100;
    libcall_params[3] = (uint32_t) &local_chain_config;

    // Clone called by Exchange, forward the request to Ethereum
    if (args != NULL) {
        if (args->id != 0x100) {
            app_exit();
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
        os_lib_call((uint32_t *) &libcall_params);
        // Ethereum should not return to us
        app_quit();
    }

    // os_lib_call will raise if Ethereum application is not installed. Do not try to recover.
    app_exit();
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
        app_quit();
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
