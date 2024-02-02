/*****************************************************************************
 *   Ledger Plugin SDK
 *   (c) 2023 Ledger SAS
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
 *****************************************************************************/

#include "eth_plugin_interface.h"
#include "lib_standard_app/swap_lib_calls.h"  // RUN_APPLICATION

// Functions implemented by the plugin
void handle_init_contract(ethPluginInitContract_t *parameters);
void handle_provide_parameter(ethPluginProvideParameter_t *parameters);
void handle_finalize(ethPluginFinalize_t *parameters);
void handle_provide_token(ethPluginProvideInfo_t *parameters);
void handle_query_contract_id(ethQueryContractID_t *parameters);
void handle_query_contract_ui(ethQueryContractUI_t *parameters);

// Calls the ethereum app.
static void call_app_ethereum() {
    unsigned int libcall_params[5];
    libcall_params[0] = (unsigned int) "Ethereum";
    libcall_params[1] = 0x100;
    libcall_params[2] = RUN_APPLICATION;
    libcall_params[3] = (unsigned int) NULL;
#ifdef HAVE_NBGL
    caller_app_t capp;
    const char name[] = APPNAME;
    nbgl_icon_details_t icon_details;
    uint8_t bitmap[sizeof(ICONBITMAP)];

    memcpy(&icon_details, &ICONGLYPH, sizeof(ICONGLYPH));
    memcpy(&bitmap, &ICONBITMAP, sizeof(bitmap));
    icon_details.bitmap = (const uint8_t *) bitmap;
    capp.name = (const char *) name;
    capp.icon = &icon_details;
    libcall_params[4] = (unsigned int) &capp;
#else
    libcall_params[4] = (unsigned int) NULL;
#endif
    os_lib_call((unsigned int *) &libcall_params);
}

// Function to dispatch calls from the ethereum app.
static void dispatch_call(int message, void *parameters) {
    if (parameters != NULL) {
        switch (message) {
            case ETH_PLUGIN_INIT_CONTRACT:
                handle_init_contract(parameters);
                break;
            case ETH_PLUGIN_PROVIDE_PARAMETER:
                handle_provide_parameter(parameters);
                break;
            case ETH_PLUGIN_FINALIZE:
                handle_finalize(parameters);
                break;
            case ETH_PLUGIN_PROVIDE_INFO:
                handle_provide_token(parameters);
                break;
            case ETH_PLUGIN_QUERY_CONTRACT_ID:
                handle_query_contract_id(parameters);
                break;
            case ETH_PLUGIN_QUERY_CONTRACT_UI:
                handle_query_contract_ui(parameters);
                break;
            default:
                PRINTF("Unhandled message %d\n", message);
                break;
        }
    } else {
        PRINTF("Received null parameters\n");
    }
}

// Low-level main for plugins.
__attribute__((section(".boot"))) int main(int arg0) {
    // Exit critical section
    __asm volatile("cpsie i");

    os_boot();

    BEGIN_TRY {
        TRY {
            // Check if plugin is called from the dashboard.
            if (!arg0) {
                // Called from dashboard, launch Ethereum app
                call_app_ethereum();

                // Will not get reached.
                __builtin_unreachable();

                os_sched_exit(-1);

            } else {
                // Not called from dashboard: called from the ethereum app!
                const unsigned int *args = (unsigned int *) arg0;

                // If `ETH_PLUGIN_CHECK_PRESENCE` is set, this means the caller is just trying to
                // know whether this app exists or not. We can skip `paraswap_plugin_call`.
                if (args[0] != ETH_PLUGIN_CHECK_PRESENCE) {
                    dispatch_call(args[0], (void *) args[1]);
                }
            }

            // Call `os_lib_end`, go back to the ethereum app.
            os_lib_end();

            // Will not get reached.
            __builtin_unreachable();
        }
        CATCH_OTHER(e) {
            PRINTF("Exiting following exception: %d\n", e);
        }
        FINALLY {
            os_lib_end();
        }
    }
    END_TRY;
}
