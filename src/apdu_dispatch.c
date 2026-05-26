/*******************************************************************************
 *   Ledger Ethereum App
 *   (c) 2016-2025 Ledger
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

#include "apdu_dispatch.h"
#include "apdu_constants.h"
#include "challenge.h"
#include "cmd_enum_value.h"
#include "cmd_field.h"
#include "cmd_get_gating.h"
#include "cmd_get_tx_simulation.h"
#include "cmd_map_entry.h"
#include "cmd_network_info.h"
#include "cmd_proxy_info.h"
#include "cmd_safe_account.h"
#include "cmd_trusted_name.h"
#include "cmd_tx_info.h"
#include "commands_712.h"
#include "commands_7702.h"
#include "manage_asset_info.h"
#include "withdrawal_index.h"

/**
 * Main APDU dispatcher.
 *
 * This function validates the APDU class byte and routes each supported
 * instruction (INS) to its dedicated handler. Handlers are responsible for
 * parsing command data, producing response bytes, and updating `*tx` with the
 * response length when applicable. The returned status word (`sw`) is sent
 * back to the host by the caller.
 *
 * Maintenance note:
 * - Keep one case per INS value.
 * - Prefer delegating parsing/business logic to dedicated handlers.
 * - Preserve existing state-reset and cleanup behavior when adding new cases.
 */
uint16_t handleApdu(command_t *cmd, uint32_t *tx) {
    uint16_t sw = SWO_NO_RESPONSE;

    if (cmd->cla != CLA) {
        return SWO_INVALID_CLA;
    }

    switch (cmd->ins) {
        case INS_GET_PUBLIC_KEY:
            forget_known_assets();
            sw = handle_get_public_key(cmd->p1, cmd->p2, cmd->data, cmd->lc, tx);
            break;

        case INS_PROVIDE_ERC20_TOKEN_INFORMATION:
            sw = handle_provide_erc20_token_information(cmd->p1, cmd->p2, cmd->lc, cmd->data, tx);
            break;

        case INS_PROVIDE_NFT_INFORMATION:
            sw = handle_provide_nft_information(cmd->p1, cmd->p2, cmd->lc, cmd->data, tx);
            break;

        case INS_SET_EXTERNAL_PLUGIN:
            sw = handle_set_external_plugin(cmd->data, cmd->lc);
            break;

        case INS_SET_PLUGIN:
            sw = handle_set_plugin(cmd->data, cmd->lc);
            break;

        case INS_PERFORM_PRIVACY_OPERATION:
            sw = handle_perform_privacy_operation(cmd->p1, cmd->p2, cmd->data, cmd->lc, tx);
            break;

        case INS_SIGN:
            sw = handle_sign(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;

        case INS_GET_APP_CONFIGURATION:
            sw = handle_get_app_configuration(tx);
            break;

        case INS_SIGN_PERSONAL_MESSAGE:
            forget_known_assets();
            sw = handle_sign_personal_message(cmd->p1, cmd->data, cmd->lc);
            break;

        case INS_SIGN_EIP_712_MESSAGE:
            switch (cmd->p2) {
                case P2_EIP712_LEGACY_IMPLEM:
                    forget_known_assets();
                    sw = handle_sign_eip712_message_v0(cmd->p1, cmd->data, cmd->lc);
                    break;
                case P2_EIP712_FULL_IMPLEM:
                    sw = handle_eip712_sign(cmd->data, cmd->lc);
                    break;
                default:
                    sw = SWO_WRONG_P1_P2;
            }
            break;

#ifdef HAVE_ETH2
        case INS_GET_ETH2_PUBLIC_KEY:
            forget_known_assets();
            sw = handle_get_eth2_public_key(cmd->p1, cmd->p2, cmd->data, cmd->lc, tx);
            break;

        case INS_SET_ETH2_WITHDRAWAL_INDEX:
            sw = handle_set_eth2_withdrawal_index(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;
#endif  // HAVE_ETH2

        case INS_EIP712_STRUCT_DEF:
            sw = handle_eip712_struct_def(cmd->p2, cmd->data, cmd->lc);
            break;

        case INS_EIP712_STRUCT_IMPL:
            sw = handle_eip712_struct_impl(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;

        case INS_EIP712_FILTERING:
            sw = handle_eip712_filtering(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;

        case INS_GET_CHALLENGE:
            sw = handle_get_challenge(tx);
            break;

        case INS_PROVIDE_TRUSTED_NAME:
            sw = handle_trusted_name(cmd->p1, cmd->data, cmd->lc);
            break;

        case INS_PROVIDE_ENUM_VALUE:
            sw = handle_enum_value(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;

        case INS_GTP_TRANSACTION_INFO:
            sw = handle_tx_info(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;

        case INS_GTP_FIELD:
            sw = handle_field(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;

        case INS_PROVIDE_PROXY_INFO:
            sw = handle_proxy_info(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;

        case INS_PROVIDE_NETWORK_CONFIGURATION:
            sw = handle_network_info(cmd->p1, cmd->p2, cmd->data, cmd->lc, tx);
            break;

#ifdef HAVE_TRANSACTION_CHECKS
        case INS_PROVIDE_TX_SIMULATION:
            sw = handle_tx_simulation(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;
#endif

        case INS_SIGN_EIP7702_AUTHORIZATION:
            sw = handle_sign_eip7702_authorization(cmd->p1, cmd->data, cmd->lc);
            break;

        case INS_PROVIDE_SAFE_ACCOUNT:
            sw = handle_safe_account(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;

        case INS_PROVIDE_GATING:
            sw = handle_gating(cmd->p1, cmd->p2, cmd->data, cmd->lc);
            break;

        case INS_PROVIDE_MAP_ENTRY:
            sw = handle_map_entry(cmd->p1, cmd->p2, cmd->lc, cmd->data);
            break;

        default:
            sw = SWO_INVALID_INS;
            break;
    }
    return sw;
}
