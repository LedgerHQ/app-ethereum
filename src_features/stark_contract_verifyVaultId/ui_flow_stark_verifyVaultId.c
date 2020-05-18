#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

void prepare_verify_vault_id_2() {
  if (contractProvisioned == CONTRACT_STARKWARE_DEPOSIT_CANCEL) {
    strcpy(strings.common.fullAddress, "Cancel Deposit");
  }
  else
  if (contractProvisioned == CONTRACT_STARKWARE_DEPOSIT_RECLAIM) {
    strcpy(strings.common.fullAddress, "Reclaim Deposit");
  }
  else
  if (contractProvisioned == CONTRACT_STARKWARE_FULL_WITHDRAWAL) {
    strcpy(strings.common.fullAddress, "Full Withdrawal");
  }  
  else
  if (contractProvisioned == CONTRACT_STARKWARE_FREEZE) {
    strcpy(strings.common.fullAddress, "Freeze");
  }    
}

void prepare_verify_vault_id_3() {
    uint8_t address[41];
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &sha3);
    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
}

void prepare_verify_vault_id_4() {
  uint8_t offset = 0;
  if ((contractProvisioned == CONTRACT_STARKWARE_DEPOSIT_CANCEL) || (contractProvisioned == CONTRACT_STARKWARE_DEPOSIT_RECLAIM)) {
    offset = 32;
  }
  snprintf(strings.common.fullAddress, 10, "%d", U4BE(dataContext.tokenContext.data, 4 + offset + 32 - 4));
}

UX_FLOW_DEF_NOCB(ux_approval_starkware_verify_vault_id_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_verify_vault_id_2_step,
    bnnn_paging,
    prepare_verify_vault_id_2(),
    {
      .title = strings.common.fullAddress,
      .text = ""
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_verify_vault_id_3_step,
    bnnn_paging,
    prepare_verify_vault_id_3(),
    {
      .title = "Contract Name",
      .text = strings.common.fullAddress,
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_verify_vault_id_4_step,
    bnnn_paging,
    prepare_verify_vault_id_4(),
    {
      .title = "Token Account",
      .text = strings.common.fullAddress
    });


UX_FLOW_DEF_NOCB(
    ux_approval_starkware_verify_vault_id_5_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_verify_vault_id_6_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_verify_vault_id_7_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_approval_starkware_verify_vault_id_flow [] = {
  &ux_approval_starkware_verify_vault_id_1_step,
  &ux_approval_starkware_verify_vault_id_2_step,
  &ux_approval_starkware_verify_vault_id_3_step,
  &ux_approval_starkware_verify_vault_id_4_step,
  &ux_approval_starkware_verify_vault_id_5_step,
  &ux_approval_starkware_verify_vault_id_6_step,
  &ux_approval_starkware_verify_vault_id_7_step,
  FLOW_END_STEP,
};

#endif

