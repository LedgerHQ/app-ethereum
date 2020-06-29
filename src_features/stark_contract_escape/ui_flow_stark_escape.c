#ifdef HAVE_STARKWARE

#include "shared_context.h"
#include "ui_callbacks.h"

void prepare_escape_3() {  
    uint8_t address[41];
    getEthAddressStringFromBinary(tmpContent.txContent.destination, address, &global_sha3, chainConfig);
    strings.common.fullAddress[0] = '0';
    strings.common.fullAddress[1] = 'x';
    os_memmove((unsigned char *)strings.common.fullAddress+2, address, 40);
    strings.common.fullAddress[42] = '\0';
}

void prepare_escape_4() {
  uint256_t amount, amountPre, quantum;
  uint8_t decimals;
  char *ticker = (char*)PIC(chainConfig->coinName);

   if (dataContext.tokenContext.quantumIndex == MAX_TOKEN) {
    decimals = WEI_TO_ETHER;
  }
  else {
    tokenDefinition_t *token = &tmpCtx.transactionContext.tokens[dataContext.tokenContext.quantumIndex];
    decimals = token->decimals;
    ticker = (char*)token->ticker;    
  }
  readu256BE(dataContext.tokenContext.data + 4 + 32 + 32 + 32, &amountPre);
  readu256BE(dataContext.tokenContext.quantum, &quantum);
  mul256(&amountPre, &quantum, &amount);
  tostring256(&amount, 10, (char*)(G_io_apdu_buffer + 100), 100);
  strcpy(strings.common.fullAmount, ticker);
  adjustDecimals((char*)(G_io_apdu_buffer + 100), strlen((char*)(G_io_apdu_buffer + 100)), strings.common.fullAmount + strlen(ticker), 50 - strlen(ticker), decimals);
}

void prepare_escape_5() {
   snprintf(strings.tmp.tmp, 70, "0x%.*H", 32, dataContext.tokenContext.data + 4 + 32);
}

void prepare_escape_6() {
 snprintf(strings.common.fullAddress, 10, "%d", U4BE(dataContext.tokenContext.data, 4 + 32 - 4));
}

UX_FLOW_DEF_NOCB(ux_approval_starkware_escape_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });

UX_FLOW_DEF_NOCB(
    ux_approval_starkware_escape_2_step,
    bnnn_paging,
    {
      .title = "Escape",
      .text = " "
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_escape_3_step,
    bnnn_paging,
    prepare_escape_3(),
    {
      .title = "Contract Name",
      .text = strings.common.fullAddress,
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_escape_4_step,
    bnnn_paging,
    prepare_escape_4(),
    {
      .title = "Amount",
      .text = strings.common.fullAmount
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_escape_5_step,
    bnnn_paging,
    prepare_escape_5(),
    {
      .title = "Master Account",
      .text = strings.tmp.tmp
    });

UX_STEP_NOCB_INIT(
    ux_approval_starkware_escape_6_step,
    bnnn_paging,
    prepare_escape_6(),
    {
      .title = "Token Account",
      .text = strings.common.fullAddress
    });

UX_FLOW_DEF_NOCB(
    ux_approval_starkware_escape_7_step,
    bnnn_paging,
    {
      .title = "Max Fees",
      .text = strings.common.maxFee,
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_escape_8_step,
    pbb,
    io_seproxyhal_touch_tx_ok(NULL),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });

UX_FLOW_DEF_VALID(
    ux_approval_starkware_escape_9_step,
    pb,
    io_seproxyhal_touch_tx_cancel(NULL),
    {
      &C_icon_crossmark,
      "Reject",
    });

const ux_flow_step_t *        const ux_approval_starkware_escape_flow [] = {
  &ux_approval_starkware_escape_1_step,
  &ux_approval_starkware_escape_2_step,
  &ux_approval_starkware_escape_3_step,
  &ux_approval_starkware_escape_4_step,
  &ux_approval_starkware_escape_5_step,
  &ux_approval_starkware_escape_6_step,
  &ux_approval_starkware_escape_7_step,
  &ux_approval_starkware_escape_8_step,
  &ux_approval_starkware_escape_9_step,
  FLOW_END_STEP,
};

#endif
