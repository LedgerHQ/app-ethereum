#ifndef _UI_FLOW_H_
#define _UI_FLOW_H_

#include "shared_context.h"

#include "os_io_seproxyhal.h"
#include "ux.h"

extern const ux_flow_step_t* const ux_idle_flow[];

extern const ux_flow_step_t* const ux_warning_contract_data_flow[];

extern const ux_flow_step_t* const ux_settings_flow[];

extern const ux_flow_step_t* const ux_display_public_flow[];

extern const ux_flow_step_t* const ux_confirm_selector_flow[];

extern const ux_flow_step_t* const ux_confirm_parameter_flow[];

extern const ux_flow_step_t* const ux_approval_allowance_flow[];

extern const ux_flow_step_t* const ux_sign_flow[];

extern const ux_flow_step_t* const ux_sign_712_v0_flow[];

extern const ux_flow_step_t* const ux_display_public_eth2_flow[];

extern const ux_flow_step_t* const ux_display_privacy_public_key_flow[];

extern const ux_flow_step_t* const ux_display_privacy_shared_secret_flow[];

extern const ux_flow_step_t* ux_approval_tx_flow[15];

#ifdef HAVE_STARKWARE

extern const ux_flow_step_t* const ux_display_stark_public_flow[];

extern const ux_flow_step_t* const ux_stark_limit_order_flow[];

extern const ux_flow_step_t* const ux_stark_transfer_flow[];

extern const ux_flow_step_t* const ux_stark_self_transfer_flow[];

extern const ux_flow_step_t* const ux_stark_transfer_conditional_flow[];

extern const ux_flow_step_t* const ux_stark_self_transfer_conditional_flow[];

extern const ux_flow_step_t* const ux_approval_starkware_register_flow[];

extern const ux_flow_step_t* const ux_approval_starkware_deposit_flow[];

extern const ux_flow_step_t* const ux_approval_starkware_withdraw_flow[];

extern const ux_flow_step_t* const ux_approval_starkware_verify_vault_id_flow[];

extern const ux_flow_step_t* const ux_approval_starkware_escape_flow[];

extern const ux_flow_step_t* const ux_approval_starkware_verify_escape_flow[];

extern const ux_flow_step_t* const ux_stark_unsafe_sign_flow[];

#endif

#endif  // _UI_FLOW_H_
