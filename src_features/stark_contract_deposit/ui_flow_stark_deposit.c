#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"
#include "utils.h"

void prepare_deposit_3() {
    uint8_t address[41];
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &sha3);
    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
}

void prepare_deposit_4() {
  snprintf(strings.common.fullAddress, 10, "%d", U4BE(dataContext.tokenContext.data, 4 + 32 + 32 - 4));
}

void prepare_deposit_5() {
  uint256_t amount, amountPre, quantum;
  uint8_t decimals;
  char *ticker = (char*)PIC(chainConfig->coinName);

  if (contractProvisioned == CONTRACT_STARKWARE_DEPOSIT_ETH) {
    decimals = WEI_TO_ETHER;
    convertUint256BE(tmpContent.txContent.value.value, tmpContent.txContent.value.length, &amountPre);
  }
  else {
    tokenDefinition_t *token = &tmpCtx.transactionContext.tokens[dataContext.tokenContext.quantumIndex];
    decimals = token->decimals;
    ticker = (char*)token->ticker;
    readu256BE(dataContext.tokenContext.data + 4 + 32 + 32, &amountPre);
  }
  readu256BE(dataContext.tokenContext.quantum, &quantum);
  mul256(&amountPre, &quantum, &amount);
  tostring256(&amount, 10, (char*)(G_io_apdu_buffer + 100), 100);
  strcpy(strings.common.fullAmount, ticker);
  adjustDecimals((char*)(G_io_apdu_buffer + 100), strlen((char*)(G_io_apdu_buffer + 100)), strings.common.fullAmount + strlen(ticker), 50 - strlen(ticker), decimals);
}

UX_FLOW_DEF_NOCB(ux_approval_starkware_deposit_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(
    ux_approval_starkware_deposit_2_step,
    bnnn_paging,
    {
      .title = "Deposit",
      .text = ""
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_deposit_3_step,
    bnnn_paging,
    prepare_deposit_3(),
    {
      .title = "Contract Name",
      .text = strings.common.fullAddress,
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_deposit_4_step,
    bnnn_paging,
    prepare_deposit_4(),
    {
      .title = "Token Account",
      .text = strings.common.fullAddress
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_deposit_5_step,
    bnnn_paging,
    prepare_deposit_5(),
    {
      .title = "Amount",
      .text = strings.common.fullAmount
    });


UX_FLOW_DEF_NOCB(
    ux_approval_starkware_deposit_6_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_deposit_7_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_deposit_8_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_approval_starkware_deposit_flow [] = {
  &ux_approval_starkware_deposit_1_step,
  &ux_approval_starkware_deposit_2_step,
  &ux_approval_starkware_deposit_3_step,
  &ux_approval_starkware_deposit_4_step,
  &ux_approval_starkware_deposit_5_step,
  &ux_approval_starkware_deposit_6_step,
  &ux_approval_starkware_deposit_7_step,
  &ux_approval_starkware_deposit_8_step,
  FLOW_END_STEP,
};

#endif

