#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

void prepare_register_4();

void prepare_withdraw_3() {
    uint8_t address[41];
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &sha3);
    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
}

void prepare_withdraw_5() {
  char *ticker = (char*)PIC(chainConfig->coinName);

  if (dataContext.tokenContext.quantumIndex != MAX_TOKEN) {
    tokenDefinition_t *token = &tmpCtx.transactionContext.tokens[dataContext.tokenContext.quantumIndex];
    ticker = (char*)token->ticker;
  }
  strcpy(strings.common.fullAmount, ticker);
}

UX_FLOW_DEF_NOCB(ux_approval_starkware_withdraw_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(
    ux_approval_starkware_withdraw_2_step,
    bnnn_paging,
    {
      .title = "Withdrawal",
      .text = " "
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_withdraw_3_step,
    bnnn_paging,
    prepare_withdraw_3(),
    {
      .title = "Contract Name",
      .text = strings.common.fullAddress,
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_withdraw_4_step,
    bnnn_paging,
    prepare_register_4(),
    {
      .title = "To Eth Address",
      .text = strings.common.fullAddress
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_withdraw_5_step,
    bnnn_paging,
    prepare_withdraw_5(),
    {
      .title = "Token Symbol",
      .text = strings.common.fullAmount
    });


UX_FLOW_DEF_NOCB(
    ux_approval_starkware_withdraw_6_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_withdraw_7_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_withdraw_8_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_approval_starkware_withdraw_flow [] = {
  &ux_approval_starkware_withdraw_1_step,
  &ux_approval_starkware_withdraw_2_step,
  &ux_approval_starkware_withdraw_3_step,
  &ux_approval_starkware_withdraw_4_step,
  &ux_approval_starkware_withdraw_5_step,
  &ux_approval_starkware_withdraw_6_step,
  &ux_approval_starkware_withdraw_7_step,
  &ux_approval_starkware_withdraw_8_step,
  FLOW_END_STEP,
};

#endif
